#include "RTDenoisePass.h"
#include "../../GUI/PathTracingSettings.h"
#include "../../../../engine/rendering/Texture2D.h"
#include "../../../../engine/util/Logger.h"
#include "../rt_globals.h"
#include "../system/KernelManager.h"
#include "../../../../engine/util/QueryManager.h"
#include "../../../../engine/rendering/Screen.h"
#include <engine/rendering/architecture/RenderPipeline.h>
#include "../../../../../third_party/RadeonRays/Calc/inc/except.h"

RTDenoisePass::RTDenoisePass()
	:RenderPass("RTDenoisePass")
{
	int width = PathTracerSettings::GI.imageResolution.value.x;
	int height = PathTracerSettings::GI.imageResolution.value.y;

	// Create denoised image
	std::shared_ptr<Texture2D> denoisedImage = std::make_shared<Texture2D>();
	denoisedImage->create(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_NEAREST, nullptr);
	GL_ERROR_CHECK();

	m_denoisedImage = std::make_shared<RTInteropTexture2D>();
	m_denoisedImage->createFromOpenGLTexture(g_clContext, CL_MEM_READ_WRITE, denoisedImage);

	setupKernels();
	KernelManager::addOnRecompilationListener([this]() { setupKernels(); });

	Screen::addResizeListener([this]() {
		m_denoisedImage->resize(Screen::getWidth(), Screen::getHeight());
	});
}

void RTDenoisePass::update()
{
	if (!PathTracerSettings::GI.useDenoise)
		return;

	try
	{
		ScopedProfiling prof("Denoise");

		int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		int imageHeight = PathTracerSettings::GI.imageResolution.value.y;

		cl_mem nextFrameImage;

		if (!m_renderPipeline->tryFetch<cl_mem>("NextFrameImageCL", nextFrameImage))
			return;

		glFinish();
		g_clContext.AcquireGLObjects(0, { nextFrameImage, m_denoisedImage->getCLMem() });

		uint32_t argc = 0;
		m_bilateralDenoiseKernel.setArg(argc++, imageWidth);
		m_bilateralDenoiseKernel.setArg(argc++, imageHeight);
		m_bilateralDenoiseKernel.setArg(argc++, PathTracerSettings::GI.denoiseKernelRadius);
		m_bilateralDenoiseKernel.setArg(argc++, PathTracerSettings::GI.bilateralDenoiseSigmaSpatial);
		m_bilateralDenoiseKernel.setArg(argc++, PathTracerSettings::GI.bilateralDenoiseSigmaRange);
		m_bilateralDenoiseKernel.setArg(argc++, nextFrameImage);
		m_bilateralDenoiseKernel.setArg(argc++, m_denoisedImage->getCLMem());

		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
		g_clContext.Launch2D(0, gs, ls, m_bilateralDenoiseKernel);

		g_clContext.ReleaseGLObjects(0, { nextFrameImage, m_denoisedImage->getCLMem() });
		g_clContext.Finish(0);

		// Set pipeline buffers
		m_renderPipeline->put<cl_mem>("NextFrameImageCL", m_denoisedImage->getCLMem());
		m_renderPipeline->putPtr<Texture2D>("NextFrameImage", m_denoisedImage->getGLTexture().get());
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

void RTDenoisePass::setupKernels()
{
	auto program = KernelManager::getProgram("Denoise", g_clContext);
	m_bilateralDenoiseKernel = program.GetKernel("BilateralDenoise");
}
