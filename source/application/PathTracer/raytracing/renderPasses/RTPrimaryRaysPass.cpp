#include "RTPrimaryRaysPass.h"
#include <engine/rendering/architecture/RenderPipeline.h>
#include "../rt_globals.h"
#include "engine/rendering/Screen.h"
#include "engine/globals.h"
#include "../util/CLHelper.h"
#include "../../../../engine/camera/FreeCameraViewController.h"
#include "../../../../engine/util/Random.h"
#include "../system/KernelManager.h"
#include <radeon_rays_cl.h>
#include "../../GUI/PathTracingSettings.h"
#include "../third_party/RadeonRays/Calc/inc/except.h"
#include "../scene/RTScene.h"
#include "../system/RTBufferManager.h"
#include "../util/RTUtil.h"

#define RT_PRIMARY_RAYS_PASS_MEMORY_RECORD_NAME std::string("RT_PRIMARY_RAYS_PASS_MEMORY_RECORD")

RTPrimaryRaysPass::RTPrimaryRaysPass()
	:RenderPass("RTPrimaryRaysPass")
{
	int width = PathTracerSettings::GI.imageResolution.value.x;
	int height = PathTracerSettings::GI.imageResolution.value.y;
	resize(width, height);

	Screen::addResizeListener([this]() {
		resize(Screen::getWidth(), Screen::getHeight());
		PathTracerSettings::GI.imageResolution.value = glm::ivec2(Screen::getWidth(), Screen::getHeight());
	});
}

void RTPrimaryRaysPass::update()
{
	if (m_hasErrors || g_requestedPause)
		return;

	auto program = KernelManager::getProgram("PathTracing", g_clContext);
	m_genRaysKernel = program.GetKernel("GeneratePerspectiveRays");

	try
	{
		auto rayBuffer = generatePrimaryRays();

		int width = PathTracerSettings::GI.imageResolution.value.x;
		int height = PathTracerSettings::GI.imageResolution.value.y;
		RadeonRays::Event* isectQueryEvent;
		g_isectApi->QueryIntersection(rayBuffer, width * height, m_isectBuffer, nullptr, &isectQueryEvent);
		isectQueryEvent->Wait();
	}
	catch (const std::exception&)
	{
		Screen::showMessageBox("RTPrimaryRaysPass: Critical Error. ", 
			"There was a critical error in RTPrimaryRaysPass. Please check the console for more info.");
		m_hasErrors = true;
		return;
	}
	catch (const Calc::Exception&)
	{
		Screen::showMessageBox("RTPrimaryRaysPass: Critical Error. ", "There was a critical error in RTPrimaryRaysPass. Please check the console for more info.");
		m_hasErrors = true;
		return;
	}

	m_renderPipeline->putPtr("RayBuffer", &m_rayBuffer);
	m_renderPipeline->putPtr("PrimaryIntersectionBufferCL", &m_isectBufferCL);
	m_renderPipeline->putPtr("PrimaryRayDifferentialsBufferCL", &m_rayDifferentialsBuffer);
}

void RTPrimaryRaysPass::resize(int width, int height)
{
	RTBufferManager::clearMemoryRecordContext(RT_PRIMARY_RAYS_PASS_MEMORY_RECORD_NAME);

	RTScopedMemoryRecord memRecord(RT_PRIMARY_RAYS_PASS_MEMORY_RECORD_NAME);

	m_rayBuffer = RTBufferManager::createBuffer<RadeonRays::ray>(CL_MEM_READ_WRITE, width * height);
	m_rayDifferentialsBuffer = RTBufferManager::createBuffer<RTRayDifferentials>(CL_MEM_READ_WRITE, width * height);
	m_isectBufferCL = RTBufferManager::createBuffer<RadeonRays::Intersection>(CL_MEM_READ_WRITE, width * height);
	m_isectBuffer = CreateFromOpenClBuffer(g_isectApi, m_isectBufferCL);
}

RadeonRays::Buffer* RTPrimaryRaysPass::generatePrimaryRays()
{
	try
	{
		int width = PathTracerSettings::GI.imageResolution.value.x;
		int height = PathTracerSettings::GI.imageResolution.value.y;

		RTPinholeCamera cam;

		float w = static_cast<float>(width);
		float h = static_cast<float>(height);
		glm::vec3 p = MainCamera->getPosition();
		float nc = MainCamera->getNearClipPlane();
		Frustum frustum = MainCamera->getFrustum();

		cam.r00 = CLHelper::toFloat3(RTUtil::screenToRay(glm::vec3(0.0f, 0.0f, nc)).direction);
		cam.r10 = CLHelper::toFloat3(RTUtil::screenToRay(glm::vec3(w, 0.0f, nc)).direction);
		cam.r11 = CLHelper::toFloat3(RTUtil::screenToRay(glm::vec3(w, h, nc)).direction);
		cam.r01 = CLHelper::toFloat3(RTUtil::screenToRay(glm::vec3(0.0f, h, nc)).direction);
		cam.pos = CLHelper::toFloat3(MainCamera->getPosition());
		cam.direction = CLHelper::toFloat3(MainCamera->getForward());
		cam.width = width;
		cam.height = height;

		auto camera = ECS::getSystem<RTScene>()->getDeviceScene().camera;
		auto writeEvt = g_clContext.WriteBuffer(0, camera, &cam, 1);
		writeEvt.Wait();

		uint32_t argc = 0;
		m_genRaysKernel.setArg(argc++, m_rayBuffer);
		m_genRaysKernel.setArg(argc++, m_rayDifferentialsBuffer);
		m_genRaysKernel.setArg(argc++, camera);

		// Run kernel
		size_t ls_div = 8;
		size_t gs[] = { static_cast<size_t>((width + ls_div - 1) / ls_div * ls_div),
			static_cast<size_t>((height + ls_div - 1) / ls_div * ls_div) };
		size_t ls[] = { ls_div, ls_div };
		g_clContext.Launch2D(0, gs, ls, m_genRaysKernel);
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

	return RadeonRays::CreateFromOpenClBuffer(g_isectApi, m_rayBuffer);
}