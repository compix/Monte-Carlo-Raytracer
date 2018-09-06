#include "RTBDPTPass.h"
#include "../rt_globals.h"
#include "glm/glm.hpp"
#include "engine/rendering/Screen.h"
#include "engine/rendering/architecture/RenderPipeline.h"
#include "engine/ecs/ECS.h"
#include "engine/geometry/Transform.h"
#include "../source/engine/rendering/Texture2D.h"
#include "../../GUI/PathTracingSettings.h"
#include "../system/KernelManager.h"
#include "../scene/RTScene.h"
#include "../source/engine/rendering/renderer/MeshRenderers.h"
#include "../source/engine/resource/ResourceManager.h"
#include "../source/engine/camera/FreeCameraViewController.h"
#include "../source/engine/globals.h"
#include "../source/engine/rendering/Framebuffer.h"
#include "../third_party/RadeonRays/RadeonRays/include/radeon_rays_cl.h"
#include "../util/CLHelper.h"
#include "RTPrimaryRaysPass.h"
#include "../source/engine/util/QueryManager.h"
#include "../third_party/RadeonRays/Calc/inc/except.h"
#include "../third_party/RadeonRays/Calc/src/except_clw.h"
#include "../system/PlatformManager.h"
#include "../system/RTBufferManager.h"
#include "../util/RTUtil.h"
#include "../source/engine/util/Timer.h"

#define RT_BDPT_MEMORY_RECORD_CONTEXT_NAME std::string("RT_BDPT_MEMORY_RECORD_CONTEXT")

RTBDPTPass::RTBDPTPass()
	:RenderPass("RTBDPTPass"), m_maxDepth(PathTracerSettings::GI.maxDepth)
{
	PathTracerSettings::GI.imageResolution.value = glm::ivec2(Screen::getWidth(), Screen::getHeight());

	ECS::getSystem<RTScene>()->addSceneUpdateListener([&]() { m_frameIndex = 0; });

	try
	{
		createBuffers();
		setupKernels();
	}
	catch (const std::exception&)
	{
		m_hasErrors = true;
		Screen::showMessageBox("BDPT: Critical Error. ",
			"There was a critical error in BDPT. Please check the console for more info.");
		return;
	}
	catch (const Calc::Exception&)
	{
		m_hasErrors = true;
		Screen::showMessageBox("BDPT: Critical Error. ",
			"There was a critical error in BDPT. Please check the console for more info.");
		return;
	}


	KernelManager::addOnRecompilationListener([this]() { setupKernels(); });

	Screen::addResizeListener([this]() {
		PathTracerSettings::GI.imageResolution.value = glm::ivec2(Screen::getWidth(), Screen::getHeight());
		createBuffers();
		m_frameIndex = 0;
	});
}

void RTBDPTPass::update()
{
	if (m_hasErrors || ECS::getSystem<RTScene>()->getDeviceScene().lights.GetElementCount() == 0)
		return;

	if (m_maxDepth != PathTracerSettings::GI.maxDepth)
	{
		m_maxDepth = PathTracerSettings::GI.maxDepth;
		createBuffers();
	}

	if (m_renderPipeline->getCamera()->getComponent<FreeCameraViewController>()->bMovedInLastUpdate)
	{
		m_frameIndex = 0;
		m_totalRenderTime = 0.0f;
	}

	g_frameIndex = m_frameIndex;

	bool stopAtTime = PathTracerSettings::DEMO.stopAtTime > 0.0f && m_totalRenderTime >= PathTracerSettings::DEMO.stopAtTime;
	g_requestedPause = stopAtTime || PathTracerSettings::DEMO.stopAtFrame == m_frameIndex;

	if (!g_requestedPause)
	{
		if (m_frameIndex > 0)
			m_totalRenderTime += Time::deltaTime();

		try
		{
			generateStartVertices();
			generateSecondaryVertices();
			prepareVertexConnections();
			makeConnections();
	
			copyRadianceBuffer();
		}
		catch (const std::exception&)
		{
			m_hasErrors = true;
			Screen::showMessageBox("BDPT: Critical Error. ", 
				"There was a critical error in BDPT. Please check the console for more info.");
			return;
		}
		catch (const Calc::Exception&)
		{
			m_hasErrors = true;
			Screen::showMessageBox("BDPT: Critical Error. ", 
				"There was a critical error in BDPT. Please check the console for more info.");
			return;
		}
	}

	// Set pipeline buffers
	m_renderPipeline->put<cl_mem>("RadianceBufferCL", m_radianceBuffer);

	if (!g_requestedPause)
	{
		++m_frameIndex;
	}

	g_totalRenderTime = m_totalRenderTime;
}

void RTBDPTPass::generateStartVertices()
{
	try
	{
		ScopedProfiling prof("BDPT:generateStartVertices");
	
		const int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		const int imageHeight = PathTracerSettings::GI.imageResolution.value.y;
	
		RTPinholeCamera cam;
	
		// Set camera
		float w = static_cast<float>(imageWidth);
		float h = static_cast<float>(imageHeight);
		MainCamera->update();
		glm::vec3 p = MainCamera->getPosition();
		float nc = MainCamera->getNearClipPlane();
		Frustum frustum = MainCamera->getFrustum();
	
		cam.r00 = CLHelper::toFloat3(RTUtil::screenToRay(glm::vec3(0.0f, 0.0f, nc)).direction);
		cam.r10 = CLHelper::toFloat3(RTUtil::screenToRay(glm::vec3(w, 0.0f, nc)).direction);
		cam.r11 = CLHelper::toFloat3(RTUtil::screenToRay(glm::vec3(w, h, nc)).direction);
		cam.r01 = CLHelper::toFloat3(RTUtil::screenToRay(glm::vec3(0.0f, h, nc)).direction);
		cam.pos = CLHelper::toFloat3(MainCamera->getPosition());
		cam.direction = CLHelper::toFloat3(MainCamera->getForward());
		cam.width = static_cast<rt_uint32>(imageWidth);
		cam.height = static_cast<rt_uint32>(imageHeight);
	
		// Compute the area of a virtual image in front of the camera on the plane at z=1 in camera coordinates.
		glm::vec3 pMin = MainCamera->ndcToCameraPoint(glm::vec3(-1.0f, -1.0f, -1.0f));
		glm::vec3 pMax = MainCamera->ndcToCameraPoint(glm::vec3(1.0f, 1.0f, -1.0f));
		pMin /= pMin.z;
		pMax /= pMax.z;
	
		cam.area = std::abs((pMax.x - pMin.x) * (pMax.y - pMin.y));
	
		cam.worldToClip = CLHelper::toMatrix(MainCamera->viewProj());
	
		auto camera = ECS::getSystem<RTScene>()->getDeviceScene().camera;
		auto writeEvt = g_clContext.WriteBuffer(0, camera, &cam, 1);
		writeEvt.Wait();
	
		uint32_t argc = setSceneArgs(m_startVerticesGenerationKernel, 0);
		argc = setImageArgs(m_startVerticesGenerationKernel, argc);

		m_startVerticesGenerationKernel.setArg(argc++, m_frameIndex);
		m_startVerticesGenerationKernel.setArg(argc++, m_maxDepth);

		// Camera params
		m_startVerticesGenerationKernel.setArg(argc++, m_cameraVertices);
		m_startVerticesGenerationKernel.setArg(argc++, m_cameraRays);
		m_startVerticesGenerationKernel.setArg(argc++, m_cameraThroughputs);
		m_startVerticesGenerationKernel.setArg(argc++, m_cameraFwdPdfs);
		m_startVerticesGenerationKernel.setArg(argc++, m_cameraVertexCounts);

		// Light params
		m_startVerticesGenerationKernel.setArg(argc++, m_lightVertices);
		m_startVerticesGenerationKernel.setArg(argc++, m_lightRays);
		m_startVerticesGenerationKernel.setArg(argc++, m_lightThroughputs);
		m_startVerticesGenerationKernel.setArg(argc++, m_lightFwdPdfs);
		m_startVerticesGenerationKernel.setArg(argc++, m_lightVertexCounts);

		m_startVerticesGenerationKernel.setArg(argc++, m_finalRadianceBuffer);

		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
		g_clContext.Launch2D(0, gs, ls, m_startVerticesGenerationKernel);
		g_clContext.Finish(0);
	
		// Query intersections
		RadeonRays::Event* camIsectQueryEvent;
		g_isectApi->QueryIntersection(RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_cameraRays), 
			imageWidth * imageHeight, RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_cameraIntersections), nullptr, &camIsectQueryEvent);
		
		RadeonRays::Event* lightIsectQueryEvent;
		g_isectApi->QueryIntersection(RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_lightRays),
			imageWidth * imageHeight, RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_lightIntersections), nullptr, &lightIsectQueryEvent);
	
		camIsectQueryEvent->Wait();
		lightIsectQueryEvent->Wait();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
	catch (const Calc::Exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
}

void RTBDPTPass::generateSecondaryVertices()
{
	try
	{
		ScopedProfiling prof("BDPT:generateSecondaryVertices");
	
		const int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		const int imageHeight = PathTracerSettings::GI.imageResolution.value.y;
	
		uint32_t argc = setSceneArgs(m_secondaryVerticesGenerationKernel, 0);
		argc = setImageArgs(m_secondaryVerticesGenerationKernel, argc);

		m_secondaryVerticesGenerationKernel.setArg(argc++, m_frameIndex);
		m_secondaryVerticesGenerationKernel.setArg(argc++, m_maxDepth);
	
		const uint32_t pathArgStartIdx = argc;
	
		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
	
		for (int depth = 1; depth <= m_maxDepth + 1; ++depth)
		{
			// Extend camera subpath
			argc = pathArgStartIdx;
			int isCameraPath = 1;
			m_secondaryVerticesGenerationKernel.setArg(argc++, isCameraPath);
			m_secondaryVerticesGenerationKernel.setArg(argc++, m_cameraVertices);
			m_secondaryVerticesGenerationKernel.setArg(argc++, m_cameraRays);
			m_secondaryVerticesGenerationKernel.setArg(argc++, m_cameraIntersections);
			m_secondaryVerticesGenerationKernel.setArg(argc++, m_cameraThroughputs);
			m_secondaryVerticesGenerationKernel.setArg(argc++, m_cameraFwdPdfs);
			m_secondaryVerticesGenerationKernel.setArg(argc++, m_cameraVertexCounts);
			m_secondaryVerticesGenerationKernel.setArg(argc++, depth);
	
			g_clContext.Launch2D(0, gs, ls, m_secondaryVerticesGenerationKernel);
	
			if (depth <= m_maxDepth)
			{
				// Extend light subpath
				argc = pathArgStartIdx;
				isCameraPath = 0;
				m_secondaryVerticesGenerationKernel.setArg(argc++, isCameraPath);
				m_secondaryVerticesGenerationKernel.setArg(argc++, m_lightVertices);
				m_secondaryVerticesGenerationKernel.setArg(argc++, m_lightRays);
				m_secondaryVerticesGenerationKernel.setArg(argc++, m_lightIntersections);
				m_secondaryVerticesGenerationKernel.setArg(argc++, m_lightThroughputs);
				m_secondaryVerticesGenerationKernel.setArg(argc++, m_lightFwdPdfs);
				m_secondaryVerticesGenerationKernel.setArg(argc++, m_lightVertexCounts);
				m_secondaryVerticesGenerationKernel.setArg(argc++, depth);
	
				g_clContext.Launch2D(0, gs, ls, m_secondaryVerticesGenerationKernel);
			}
	
			// Camera and light paths can run in parallel, it's thus enough to have one sync point after both kernels.
			// TODO: This can be potentially improved by separating the subpath generation kernels into a thread each and just wait
			// when both are done. This distributes the time to wait over both kernels more evenly.
			g_clContext.Finish(0);
	
			// Query intersections
			RadeonRays::Event* camIsectQueryEvent;
			g_isectApi->QueryIntersection(RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_cameraRays),
				imageWidth * imageHeight, RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_cameraIntersections), nullptr, &camIsectQueryEvent);
	
			if (depth <= m_maxDepth)
			{
				RadeonRays::Event* lightIsectQueryEvent;
				g_isectApi->QueryIntersection(RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_lightRays),
					imageWidth * imageHeight, RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_lightIntersections), nullptr, &lightIsectQueryEvent);
				lightIsectQueryEvent->Wait();
			}
	
			camIsectQueryEvent->Wait();
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
	catch (const Calc::Exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
}

void RTBDPTPass::prepareVertexConnections()
{
	try
	{
		ScopedProfiling prof("BDPT:prepareVertexConnections");

		// Compute first results and request connection visibilities

		const int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		const int imageHeight = PathTracerSettings::GI.imageResolution.value.y;

		uint32_t argc = setSceneArgs(m_prepareConnectionsKernel, 0);
		argc = setImageArgs(m_prepareConnectionsKernel, argc);

		m_prepareConnectionsKernel.setArg(argc++, m_frameIndex);

		// Kernel specific params
		m_prepareConnectionsKernel.setArg(argc++, m_maxDepth);
		m_prepareConnectionsKernel.setArg(argc++, m_cameraVertices);
		m_prepareConnectionsKernel.setArg(argc++, m_lightVertices);
		m_prepareConnectionsKernel.setArg(argc++, m_sampledCameraVertices);
		m_prepareConnectionsKernel.setArg(argc++, m_sampledLightVertices);
		m_prepareConnectionsKernel.setArg(argc++, m_connectionRays);
		m_prepareConnectionsKernel.setArg(argc++, m_cameraVertexCounts);
		m_prepareConnectionsKernel.setArg(argc++, m_lightVertexCounts);
		m_prepareConnectionsKernel.setArg(argc++, m_tempRadianceBuffer);

		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
		g_clContext.Launch2D(0, gs, ls, m_prepareConnectionsKernel);
		g_clContext.Finish(0);

		// Query occlusions
		RadeonRays::Event* evt;
		g_isectApi->QueryOcclusion(RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_connectionRays),
			imageWidth * imageHeight * getMaxPossibleConnectionsCount(),
			RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_connectionVisibilities), nullptr, &evt);

		evt->Wait();
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
	catch (const Calc::Exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
}

void RTBDPTPass::makeConnections()
{
	try
	{
		ScopedProfiling prof("BDPT:makeConnections");
	
		const int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		const int imageHeight = PathTracerSettings::GI.imageResolution.value.y;
		
		uint32_t argc = setSceneArgs(m_connectionKernel, 0);
		argc = setImageArgs(m_connectionKernel, argc);
		m_connectionKernel.setArg(argc++, m_frameIndex);
	
		// Kernel specific params
		m_connectionKernel.setArg(argc++, m_maxDepth);
		m_connectionKernel.setArg(argc++, m_cameraVertices);
		m_connectionKernel.setArg(argc++, m_lightVertices);
		m_connectionKernel.setArg(argc++, m_sampledCameraVertices);
		m_connectionKernel.setArg(argc++, m_sampledLightVertices);
		m_connectionKernel.setArg(argc++, m_connectionRays);
		m_connectionKernel.setArg(argc++, m_connectionVisibilities);
		m_connectionKernel.setArg(argc++, m_cameraVertexCounts);
		m_connectionKernel.setArg(argc++, m_lightVertexCounts);
		m_connectionKernel.setArg(argc++, m_tempRadianceBuffer);
		m_connectionKernel.setArg(argc++, m_finalRadianceBuffer);
	
		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
		g_clContext.Launch2D(0, gs, ls, m_connectionKernel);
		g_clContext.Finish(0);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
	catch (const Calc::Exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
}

int RTBDPTPass::getMaxPossibleConnectionsCount()
{
	int t = m_maxDepth + 2;
	return t * (t + 1) / 2 - 2;
}

void RTBDPTPass::copyRadianceBuffer()
{
	try
	{
		ScopedProfiling prof("BDPT:copyRadianceBuffer");
		
		int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		int imageHeight = PathTracerSettings::GI.imageResolution.value.y;
	
		uint32_t argc = 0;
		m_copyBufferKernel.setArg(argc++, imageWidth);
		m_copyBufferKernel.setArg(argc++, imageHeight);
		m_copyBufferKernel.setArg(argc++, m_finalRadianceBuffer);
		m_copyBufferKernel.setArg(argc++, m_radianceBuffer);
	
		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
		g_clContext.Launch2D(0, gs, ls, m_copyBufferKernel);
		g_clContext.Finish(0);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
	catch (const Calc::Exception& e)
	{
		LOG_ERROR(e.what());
		throw;
	}
}

void RTBDPTPass::createBuffers()
{
	RTScopedMemoryRecord memRecord(RT_BDPT_MEMORY_RECORD_CONTEXT_NAME);

	try
	{
		const int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		const int imageHeight = PathTracerSettings::GI.imageResolution.value.y;

		m_radianceBuffer = RTBufferManager::createBuffer<RadeonRays::float4>(CL_MEM_READ_WRITE, imageWidth * imageHeight);
		m_finalRadianceBuffer = RTBufferManager::createBuffer<float>(CL_MEM_READ_WRITE, imageWidth * imageHeight * 3);

		int numPaths = imageWidth * imageHeight;

		m_cameraVertices = RTBufferManager::createBuffer<RTBDPTVertex>(CL_MEM_READ_WRITE, numPaths * (m_maxDepth + 2));
		m_lightVertices = RTBufferManager::createBuffer<RTBDPTVertex>(CL_MEM_READ_WRITE, numPaths * (m_maxDepth + 1));

		m_cameraRays = RTBufferManager::createBuffer<RadeonRays::ray>(CL_MEM_READ_WRITE, numPaths);
		m_cameraIntersections = RTBufferManager::createBuffer<RadeonRays::Intersection>(CL_MEM_READ_WRITE, numPaths);
		m_cameraRayDifferentials = RTBufferManager::createBuffer<RTRayDifferentials>(CL_MEM_READ_WRITE, numPaths);
		m_cameraThroughputs = RTBufferManager::createBuffer<RadeonRays::float3>(CL_MEM_READ_WRITE, numPaths);
		m_cameraFwdPdfs = RTBufferManager::createBuffer<float>(CL_MEM_READ_WRITE, numPaths);
		m_cameraVertexCounts = RTBufferManager::createBuffer<int>(CL_MEM_READ_WRITE, numPaths);

		m_lightRays = RTBufferManager::createBuffer<RadeonRays::ray>(CL_MEM_READ_WRITE, numPaths);
		m_lightIntersections = RTBufferManager::createBuffer<RadeonRays::Intersection>(CL_MEM_READ_WRITE, numPaths);
		m_lightThroughputs = RTBufferManager::createBuffer<RadeonRays::float3>(CL_MEM_READ_WRITE, numPaths);
		m_lightFwdPdfs = RTBufferManager::createBuffer<float>(CL_MEM_READ_WRITE, numPaths);
		m_lightVertexCounts = RTBufferManager::createBuffer<int>(CL_MEM_READ_WRITE, numPaths);

		m_connectionRays = RTBufferManager::createBuffer<RadeonRays::ray>(CL_MEM_READ_WRITE, numPaths * getMaxPossibleConnectionsCount());
		m_connectionVisibilities = RTBufferManager::createBuffer<int>(CL_MEM_READ_WRITE, numPaths * getMaxPossibleConnectionsCount());

		m_sampledCameraVertices = RTBufferManager::createBuffer<RTBDPTVertex>(CL_MEM_READ_WRITE, numPaths * m_maxDepth);
		m_sampledLightVertices = RTBufferManager::createBuffer<RTBDPTVertex>(CL_MEM_READ_WRITE, numPaths * m_maxDepth);

		m_tempRadianceBuffer = RTBufferManager::createBuffer<RadeonRays::float4>(CL_MEM_READ_WRITE, numPaths * getMaxPossibleConnectionsCount());
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
	}
	catch (const Calc::Exception& e)
	{
		LOG_ERROR(e.what());
	}
}

void RTBDPTPass::setupKernels()
{
	auto bdptProgram = KernelManager::getProgram("BDPT", g_clContext);
	m_copyBufferKernel = bdptProgram.GetKernel("CopyBuffer");
	m_startVerticesGenerationKernel = bdptProgram.GetKernel("GenerateStartVertices");
	m_secondaryVerticesGenerationKernel = bdptProgram.GetKernel("GenerateSecondaryVertices");
	m_connectionKernel = bdptProgram.GetKernel("ConnectVertices");
	m_prepareConnectionsKernel = bdptProgram.GetKernel("PrepareConnections");
}

int RTBDPTPass::setSceneArgs(RTKernel& kernel, int sceneArgsStart)
{
	return ECS::getSystem<RTScene>()->setSceneArgs(kernel, sceneArgsStart);
}

int RTBDPTPass::setImageArgs(RTKernel& kernel, int argsStart)
{
	kernel.setArg(argsStart++, PathTracerSettings::GI.imageResolution.value.x);
	kernel.setArg(argsStart++, PathTracerSettings::GI.imageResolution.value.y);

	return argsStart;
}
