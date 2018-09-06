#include "RTPathTracingPass.h"
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
#include "../third_party/RadeonRays/Calc/inc/except.h"
#include "../system/RTBufferManager.h"
#include "../source/engine/util/Timer.h"

#define RT_PATH_TRACING_PASS_MEMORY_RECORD_NAME std::string("RT_PATH_TRACING_PASS_MEMORY_RECORD")

RTPathTracingPass::RTPathTracingPass()
	:RenderPass("RTPathTracingPass")
{
	PathTracerSettings::GI.imageResolution.value = glm::ivec2(Screen::getWidth(), Screen::getHeight());

	createBuffers();

	ECS::getSystem<RTScene>()->addSceneUpdateListener([&](){ m_frameIndex = 0; });

	Screen::addResizeListener([this]() {
		PathTracerSettings::GI.imageResolution.value = glm::ivec2(Screen::getWidth(), Screen::getHeight());
		createBuffers();
		m_frameIndex = 0;
	});
}

void RTPathTracingPass::update()
{
	if (m_hasErrors || ECS::getSystem<RTScene>()->getDeviceScene().lights.GetElementCount() == 0)
		return;

	auto program = KernelManager::getProgram("PathTracing", g_clContext);
	m_kernel = program.GetKernel("PathTracing");
	m_shadowKernel = program.GetKernel("ShadowPass");

	if (m_renderPipeline->getCamera()->getComponent<FreeCameraViewController>()->bMovedInLastUpdate)
	{
		m_frameIndex = 0;
		m_totalRenderTime = 0.0f;
	}

	g_frameIndex = m_frameIndex;
	bool stopAtTime = PathTracerSettings::DEMO.stopAtTime > 0.0f && m_totalRenderTime >= PathTracerSettings::DEMO.stopAtTime;

	g_requestedPause = stopAtTime || m_frameIndex == PathTracerSettings::DEMO.stopAtFrame;

	try
	{
		if (!g_requestedPause)
		{
			if (m_frameIndex > 0)
				m_totalRenderTime += Time::deltaTime();

			auto isectPtr = m_renderPipeline->fetchPtr<CLWBuffer<RadeonRays::Intersection>>("PrimaryIntersectionBufferCL");
			auto rayBuffer = m_renderPipeline->fetchPtr<CLWBuffer<RadeonRays::ray>>("RayBuffer");

			const int maxDepth = PathTracerSettings::GI.maxDepth;
			for (int i = 0; i < maxDepth; ++i)
			{
				m_bounceCounter = i;
				applyShading(*isectPtr);
				applyVisibilityTest();

				if (i + 1 < maxDepth)
				{
					int width = PathTracerSettings::GI.imageResolution.value.x;
					int height = PathTracerSettings::GI.imageResolution.value.y;
					RadeonRays::Event* isectQueryEvent;
					g_isectApi->QueryIntersection(RadeonRays::CreateFromOpenClBuffer(g_isectApi, *rayBuffer), width * height,
						RadeonRays::CreateFromOpenClBuffer(g_isectApi, *isectPtr), nullptr, &isectQueryEvent);
					isectQueryEvent->Wait();
				}
			}
		}
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		m_hasErrors = true;
		Screen::showMessageBox("RTPathTracingPass: Critical Error. ", 
			"There was a critical error in RTPathTracingPass. Please check the console for more info.");
		return;
	}
	catch (const Calc::Exception& e)
	{
		LOG_ERROR(e.what());
		m_hasErrors = true;
		Screen::showMessageBox("RTPathTracingPass: Critical Error. ", 
			"There was a critical error in RTPathTracingPass. Please check the console for more info.");
		return;
	}

	m_renderPipeline->put<cl_mem>("RadianceBufferCL", m_radianceBuffer);

	if (!g_requestedPause)
	{
		++m_frameIndex;
	}

	g_totalRenderTime = m_totalRenderTime;
}

void RTPathTracingPass::applyShading(const CLWBuffer<RadeonRays::Intersection> &isect)
{
	try
	{
		int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		int imageHeight = PathTracerSettings::GI.imageResolution.value.y;

		auto rayBuffer = m_renderPipeline->fetchPtr<CLWBuffer<RadeonRays::ray>>("RayBuffer");
		if (!rayBuffer)
			throw std::runtime_error("Expected valid ray buffer but got nullptr.");

		uint32_t argc = ECS::getSystem<RTScene>()->setSceneArgs(m_kernel, 0);

		m_kernel.setArg(argc++, imageWidth);
		m_kernel.setArg(argc++, imageHeight);
		
		// Trace params
		m_kernel.setArg(argc++, m_shadowRayBuffer);
		m_kernel.setArg(argc++, *rayBuffer);
		m_kernel.setArg(argc++, isect);

		// Integrator params
		m_kernel.setArg(argc++, m_frameIndex);
		m_kernel.setArg(argc++, PathTracerSettings::GI.maxDepth);
		m_kernel.setArg(argc++, m_bounceCounter);
		m_kernel.setArg(argc++, m_tempRadianceBuffer);
		m_kernel.setArg(argc++, m_throughputBuffer);

		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
		g_clContext.Launch2D(0, gs, ls, m_kernel);
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

void RTPathTracingPass::applyVisibilityTest()
{
	try
	{
		int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		int imageHeight = PathTracerSettings::GI.imageResolution.value.y;

#ifdef RT_ENABLE_SHADOWS
		RadeonRays::Event* queryEvent;
		g_isectApi->QueryOcclusion(RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_shadowRayBuffer),
			imageWidth * imageHeight, m_shadowRayOcclusionBuffer, nullptr, &queryEvent);

		if (queryEvent)
			queryEvent->Wait();
#endif

		uint32_t argc = 0;
		m_shadowKernel.setArg(argc++, m_shadowRayBuffer);
		m_shadowKernel.setArg(argc++, m_shadowRayOcclusionBufferCL);
		m_shadowKernel.setArg(argc++, imageWidth);
		m_shadowKernel.setArg(argc++, imageHeight);
		m_shadowKernel.setArg(argc++, m_bounceCounter);
		m_shadowKernel.setArg(argc++, m_throughputBuffer);
		m_shadowKernel.setArg(argc++, m_tempRadianceBuffer);
		m_shadowKernel.setArg(argc++, m_radianceBuffer);

		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
		g_clContext.Launch2D(0, gs, ls, m_shadowKernel);
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

void RTPathTracingPass::createBuffers()
{
	RTScopedMemoryRecord memRecord(RT_PATH_TRACING_PASS_MEMORY_RECORD_NAME);

	int width = PathTracerSettings::GI.imageResolution.value.x;
	int height = PathTracerSettings::GI.imageResolution.value.y;

	m_radianceBuffer = RTBufferManager::createBuffer<RadeonRays::float4>(CL_MEM_READ_WRITE, width * height);
	m_tempRadianceBuffer = RTBufferManager::createBuffer<RadeonRays::float4>(CL_MEM_READ_WRITE, width * height);
	m_throughputBuffer = RTBufferManager::createBuffer<RTThroughput>(CL_MEM_READ_WRITE, width * height);

	m_shadowRayBuffer = RTBufferManager::createBuffer<RadeonRays::ray>(CL_MEM_READ_WRITE, width * height);
	m_shadowRayOcclusionBufferCL = RTBufferManager::createBuffer<int>(CL_MEM_READ_WRITE, width * height);
	m_shadowRayOcclusionBuffer = RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_shadowRayOcclusionBufferCL);
}
