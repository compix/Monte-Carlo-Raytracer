#pragma once
#include "RTKernel.h"
#include <CLW.h>
#include <engine/rendering/Texture2D.h>

class RTNoiseGenerationKernel : public RTKernel
{
public:
	std::shared_ptr<Texture2D> generateNoiseTexture(CLWContext context, 
		uint32_t width, uint32_t height, uint32_t numChannels);
private:
	bool m_programCreated = false;
	CLWProgram m_program;
};