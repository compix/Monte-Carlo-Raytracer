#include "RTNoiseGenerationKernel.h"
#include <engine/util/Logger.h>
#include <radeon_rays.h>
#include <radeon_rays_cl.h>

std::shared_ptr<Texture2D> RTNoiseGenerationKernel::generateNoiseTexture(CLWContext context, 
	uint32_t width, uint32_t height, uint32_t numChannels)
{
	// Create program and get m_kernel
	if (!m_programCreated)
	{
		try 
		{
			std::string programPath = std::string(ASSET_ROOT_FOLDER) + std::string("kernels/rng.cl");
			m_program = CLWProgram::CreateFromFile(programPath.c_str(), nullptr, context);
			m_kernel = m_program.GetKernel("generateNoiseTexture");
			m_programCreated = true;
		}
		catch (std::exception& e)
		{
			LOG_ERROR(e.what());
			return std::shared_ptr<Texture2D>();
		}
	}

	auto noiseTex = std::make_shared<Texture2D>();

	// Create cl tex buffer
	CLWBuffer<unsigned char> texBuffer_cl = CLWBuffer<unsigned char>::Create(context, CL_MEM_WRITE_ONLY, numChannels * width * height);

	// Run m_kernel

	uint32_t argc = 0;
	setArg(argc++, width);
	setArg(argc++, height);
	setArg(argc++, numChannels);
	setArg(argc++, texBuffer_cl);

	const int ls_div = 8;
	size_t gs[] = { static_cast<size_t>((width + ls_div - 1) / ls_div * ls_div),
					static_cast<size_t>((height + ls_div - 1) / ls_div * ls_div) };
	size_t ls[] = { ls_div, ls_div };
	context.Launch2D(0, gs, ls, m_kernel);
	context.Flush(0);

	// Map to host memory
	unsigned char* pixels = nullptr;
	auto event = context.MapBuffer(0, texBuffer_cl, RadeonRays::kMapRead, 0, numChannels * width * height * sizeof(unsigned char), &pixels);
	event.Wait();

	// Return OpenGL noise texture
	switch (numChannels)
	{
	case 1:
		noiseTex->create(width, height, GL_R8, GL_RED, GL_UNSIGNED_BYTE, Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_NEAREST, pixels);
		break;
	case 2:
		noiseTex->create(width, height, GL_RG8, GL_RG, GL_UNSIGNED_BYTE, Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_NEAREST, pixels);
		break;
	case 3:
		noiseTex->create(width, height, GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE, Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_NEAREST, pixels);
		break;
	case 4:
		noiseTex->create(width, height, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE, Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_NEAREST, pixels);
		break;
	}
	GL_ERROR_CHECK();

	event = context.UnmapBuffer(0, texBuffer_cl, pixels);
	event.Wait();

	return noiseTex;
}
