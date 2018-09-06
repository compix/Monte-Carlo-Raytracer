#include "RTToneMappingPass.h"
#include "../../../../engine/rendering/architecture/RenderPipeline.h"
#include "../../../../engine/util/QueryManager.h"
#include "../../../../engine/rendering/Texture2D.h"
#include "../system/KernelManager.h"
#include "../rt_globals.h"
#include "../../../../engine/rendering/Screen.h"
#include "../../GUI/PathTracingSettings.h"

#include "../../../../../third_party/RadeonRays/Calc/inc/except.h"
#include "../textures/RTInteropTexture2D.h"
RTToneMappingPass::RTToneMappingPass()
	:RenderPass("RTToneMappingPass")
{
	int width = PathTracerSettings::GI.imageResolution.value.x;
	int height = PathTracerSettings::GI.imageResolution.value.y;

	auto tonemappedImage = std::make_shared<Texture2D>();
	tonemappedImage->create(width, height, GL_RGBA32F, GL_RGBA, GL_FLOAT, Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_NEAREST, nullptr);
	GL_ERROR_CHECK();

	m_tonemappedImage = std::make_shared<RTInteropTexture2D>();
	m_tonemappedImage->createFromOpenGLTexture(g_clContext, CL_MEM_READ_WRITE, tonemappedImage);

	setupKernels();
	KernelManager::addOnRecompilationListener([this]() { setupKernels(); });

	Screen::addResizeListener([this]() {
		resize(Screen::getWidth(), Screen::getHeight());
	});
}

void RTToneMappingPass::update()
{
	applyReinhardToneMapping();
}

void RTToneMappingPass::applyReinhardToneMapping()
{
	if (!PathTracerSettings::GI.useTonemapping)
		return;

	try
	{
		ScopedProfiling prof("Tonemapping");
		cl_mem nextFrameImage;

		if (!m_renderPipeline->tryFetch<cl_mem>("NextFrameImageCL", nextFrameImage))
			return;

		int imageWidth = PathTracerSettings::GI.imageResolution.value.x;
		int imageHeight = PathTracerSettings::GI.imageResolution.value.y;
		glFinish();
		g_clContext.AcquireGLObjects(0, { nextFrameImage, m_tonemappedImage->getCLMem() });

		// Run tonemapping kernel
		int argc = 0;
		m_reinhardToneMappingKernel.setArg(argc++, imageWidth);
		m_reinhardToneMappingKernel.setArg(argc++, imageHeight);
		m_reinhardToneMappingKernel.setArg(argc++, PathTracerSettings::GI.minLuminance);
		m_reinhardToneMappingKernel.setArg(argc++, nextFrameImage);
		m_reinhardToneMappingKernel.setArg(argc++, m_tonemappedImage->getCLMem());

		size_t gs[] = { static_cast<size_t>((imageWidth + 7) / 8 * 8), static_cast<size_t>((imageHeight + 7) / 8 * 8) };
		size_t ls[] = { 8, 8 };
		g_clContext.Launch2D(0, gs, ls, m_reinhardToneMappingKernel);

		g_clContext.ReleaseGLObjects(0, { nextFrameImage, m_tonemappedImage->getCLMem() });
		g_clContext.Finish(0);

		// Set pipeline buffers
		m_renderPipeline->put<cl_mem>("NextFrameImageCL", m_tonemappedImage->getCLMem());
		m_renderPipeline->putPtr<Texture2D>("NextFrameImage", m_tonemappedImage->getGLTexture().get());
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

void RTToneMappingPass::setupKernels()
{
	auto tonemappingProgram = KernelManager::getProgram("ToneMapping", g_clContext);
	m_reinhardToneMappingKernel = tonemappingProgram.GetKernel("ReinhardToneMapping");
}

void RTToneMappingPass::resize(int width, int height)
{
	m_tonemappedImage->resize(width, height);
}
