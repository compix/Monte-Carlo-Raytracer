#include "RTReconstructionPass.h"
#include "../system/RTBufferManager.h"
#include "../../GUI/PathTracingSettings.h"
#include "../../../../../third_party/RadeonRays/Calc/inc/except.h"
#include "../system/KernelManager.h"
#include "../../../../engine/rendering/Framebuffer.h"
#include "../rt_globals.h"
#include "../../../../engine/camera/FreeCameraViewController.h"
#include "../../../../engine/rendering/architecture/RenderPipeline.h"

RTReconstructionPass::RTReconstructionPass()
	:RenderPass("RTReconstructionPass")
{
	// Create a 2D OpenGL floating point texture
	int width = PathTracerSettings::GI.imageResolution.value.x;
	int height = PathTracerSettings::GI.imageResolution.value.y;
	m_framebuffer = std::make_unique<Framebuffer>();
	m_framebuffer->resize(width, height);
	m_framebuffer->bind();
	auto frameTexture = std::make_shared<Texture2D>();
	frameTexture->create(width, height,
		GL_RGBA32F, GL_RGBA, GL_FLOAT, Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_NEAREST, nullptr);
	GL_ERROR_CHECK();
	m_framebuffer->attachRenderTexture2D(frameTexture);

	m_frameImage = std::make_shared<RTInteropTexture2D>();
	m_frameImage->createFromOpenGLTexture(g_clContext, CL_MEM_READ_WRITE, frameTexture);
	m_framebuffer->setDrawBuffers();
	m_framebuffer->checkFramebufferStatus();
	m_framebuffer->unbind();

	createBuffers();

	Screen::addResizeListener([this]() {
		resize(Screen::getWidth(), Screen::getHeight());
		PathTracerSettings::GI.imageResolution.value = glm::ivec2(Screen::getWidth(), Screen::getHeight());
		createBuffers();
	});
}

void RTReconstructionPass::update()
{
	setupKernels();

	if (!g_requestedPause)
	{
		try
		{
			updateReconstruction();
			//updateReconstructionAllFilters();
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

	//copyReconstructionResult();

	// Set pipeline buffers
	m_renderPipeline->put<cl_mem>("NextFrameImageCL", m_frameImage->getCLMem());
	m_renderPipeline->putPtr<Texture2D>("NextFrameImage", m_frameImage->getGLTexture().get());
}

void RTReconstructionPass::updateReconstruction()
{
	cl_mem radianceBuffer;

	if (!m_renderPipeline->tryFetch<cl_mem>("RadianceBufferCL", radianceBuffer))
		return;

	int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
	int imageHeight = PathTracerSettings::GI.imageResolution.value.y;

	auto image = m_frameImage->getCLMem();
	glFinish();
	g_clContext.AcquireGLObjects(0, { image });

	// Fill filter properties and update device data.
	RTFilterProperties filterProperties;
	filterProperties.filterType = PathTracerSettings::GI.filterSettings.filterType.curItem;
	filterProperties.radius.x = PathTracerSettings::GI.filterSettings.radius.value.x;
	filterProperties.radius.y = PathTracerSettings::GI.filterSettings.radius.value.y;
	filterProperties.mitchellB = PathTracerSettings::GI.filterSettings.mitchellB;
	filterProperties.mitchellC = PathTracerSettings::GI.filterSettings.mitchellC;

	filterProperties.lanczosSincTau = PathTracerSettings::GI.filterSettings.lanczosSincTau;

	filterProperties.gaussianAlpha = PathTracerSettings::GI.filterSettings.gaussianAlpha;
	filterProperties.gaussianExpX = PathTracerSettings::GI.filterSettings.gaussianExpX;
	filterProperties.gaussianExpY = PathTracerSettings::GI.filterSettings.gaussianExpY;

	filterProperties.pixelOffset.x = PathTracerSettings::GI.filterSettings.curPixelOffset.x;
	filterProperties.pixelOffset.y = PathTracerSettings::GI.filterSettings.curPixelOffset.y;

	auto writeEvt = g_clContext.WriteBuffer(0, m_filterProperties, &filterProperties, 1);
	writeEvt.Wait();

	uint32_t argc = 0;
	m_reconstructionKernel.setArg(argc++, imageWidth);
	m_reconstructionKernel.setArg(argc++, imageHeight);
	m_reconstructionKernel.setArg(argc++, g_frameIndex);

	m_reconstructionKernel.setArg(argc++, m_filterProperties);

	m_reconstructionKernel.setArg(argc++, radianceBuffer);
	m_reconstructionKernel.setArg(argc++, m_weightedRadianceBuffer);
	m_reconstructionKernel.setArg(argc++, m_filterWeightsBuffer);
	m_reconstructionKernel.setArg(argc++, image);

	size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
	size_t ls[] = { 8, 8 };
	g_clContext.Launch2D(0, gs, ls, m_reconstructionKernel);

	g_clContext.ReleaseGLObjects(0, { image });
	g_clContext.Finish(0);
}

void RTReconstructionPass::updateReconstructionAllFilters()
{
	cl_mem radianceBuffer;

	if (!m_renderPipeline->tryFetch<cl_mem>("RadianceBufferCL", radianceBuffer))
		return;

	int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
	int imageHeight = PathTracerSettings::GI.imageResolution.value.y;

	// Fill filter properties and update device data.
	RTFilterProperties filterProperties;
	filterProperties.filterType = PathTracerSettings::GI.filterSettings.filterType.curItem;
	filterProperties.radius.x = PathTracerSettings::GI.filterSettings.radius.value.x;
	filterProperties.radius.y = PathTracerSettings::GI.filterSettings.radius.value.y;
	filterProperties.mitchellB = PathTracerSettings::GI.filterSettings.mitchellB;
	filterProperties.mitchellC = PathTracerSettings::GI.filterSettings.mitchellC;

	filterProperties.lanczosSincTau = PathTracerSettings::GI.filterSettings.lanczosSincTau;

	filterProperties.gaussianAlpha = PathTracerSettings::GI.filterSettings.gaussianAlpha;
	filterProperties.gaussianExpX = PathTracerSettings::GI.filterSettings.gaussianExpX;
	filterProperties.gaussianExpY = PathTracerSettings::GI.filterSettings.gaussianExpY;

	filterProperties.pixelOffset.x = PathTracerSettings::GI.filterSettings.curPixelOffset.x;
	filterProperties.pixelOffset.y = PathTracerSettings::GI.filterSettings.curPixelOffset.y;

	auto writeEvt = g_clContext.WriteBuffer(0, m_filterProperties, &filterProperties, 1);
	writeEvt.Wait();

	uint32_t argc = 0;
	m_reconstructionAllFiltersKernel.setArg(argc++, imageWidth);
	m_reconstructionAllFiltersKernel.setArg(argc++, imageHeight);
	m_reconstructionAllFiltersKernel.setArg(argc++, g_frameIndex);

	m_reconstructionAllFiltersKernel.setArg(argc++, m_filterProperties);

	m_reconstructionAllFiltersKernel.setArg(argc++, radianceBuffer);
	m_reconstructionAllFiltersKernel.setArg(argc++, m_weightedRadianceBufferAllFilters);
	m_reconstructionAllFiltersKernel.setArg(argc++, m_filterWeightsBufferAllFilters);

	size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
	size_t ls[] = { 8, 8 };
	g_clContext.Launch2D(0, gs, ls, m_reconstructionAllFiltersKernel);
	g_clContext.Finish(0);
}

void RTReconstructionPass::copyReconstructionResult()
{
	int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
	int imageHeight = PathTracerSettings::GI.imageResolution.value.y;

	auto image = m_frameImage->getCLMem();
	glFinish();
	g_clContext.AcquireGLObjects(0, { image });

	uint32_t argc = 0;
	m_copyReconstructionResultKernel.setArg(argc++, imageWidth);
	m_copyReconstructionResultKernel.setArg(argc++, imageHeight);
	int curFilterIdx = PathTracerSettings::GI.filterSettings.filterType.curItem;
	m_copyReconstructionResultKernel.setArg(argc++, curFilterIdx);

	m_copyReconstructionResultKernel.setArg(argc++, m_weightedRadianceBufferAllFilters);
	m_copyReconstructionResultKernel.setArg(argc++, m_filterWeightsBufferAllFilters);
	m_copyReconstructionResultKernel.setArg(argc++, image);

	size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
	size_t ls[] = { 8, 8 };
	g_clContext.Launch2D(0, gs, ls, m_copyReconstructionResultKernel);

	g_clContext.ReleaseGLObjects(0, { image });
	g_clContext.Finish(0);
}

void RTReconstructionPass::setupKernels()
{
	auto program = KernelManager::getProgram("Reconstruction", g_clContext);
	m_reconstructionKernel = program.GetKernel("ReconstructionPass");
	m_reconstructionAllFiltersKernel = program.GetKernel("ReconstructionPassAllFilters");
	m_copyReconstructionResultKernel = program.GetKernel("CopyReconstructionResult");
}

void RTReconstructionPass::createBuffers()
{
	int width = PathTracerSettings::GI.imageResolution.value.x;
	int height = PathTracerSettings::GI.imageResolution.value.y;

	m_filterWeightsBuffer = RTBufferManager::createBuffer<float>(CL_MEM_READ_WRITE, width * height);
	m_weightedRadianceBuffer = RTBufferManager::createBuffer<RadeonRays::float4>(CL_MEM_READ_WRITE, width * height);

	m_filterWeightsBufferAllFilters = RTBufferManager::createBuffer<float>(CL_MEM_READ_WRITE, width * height * RT_NUM_FILTERS);
	m_weightedRadianceBufferAllFilters = RTBufferManager::createBuffer<RadeonRays::float4>(CL_MEM_READ_WRITE, width * height * RT_NUM_FILTERS);

	m_filterProperties = RTBufferManager::createBuffer<RTFilterProperties>(CL_MEM_READ_WRITE, 1);
}

void RTReconstructionPass::resize(int width, int height)
{
	m_frameImage->resize(width, height);
}

void RTReconstructionPass::clearFrameTextures()
{
	m_framebuffer->begin();
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	m_framebuffer->end();
}
