#pragma once
#include <engine/rendering/architecture/RenderPass.h>
#include <radeon_rays.h>
#include <CLW.h>
#include "engine/geometry/Ray.h"
#include "kernel_data.h"
#include "../kernels/RTKernel.h"
#include "../../../../engine/rendering/Texture2D.h"
#include "../textures/RTInteropTexture2D.h"

#define RT_NUM_FILTERS 5

class RTReconstructionPass : public RenderPass
{
public:
	RTReconstructionPass();

	virtual void update() override;

	void resize(int width, int height);

	void clearFrameTextures();

protected:
	void updateReconstruction();
	void updateReconstructionAllFilters();
	void copyReconstructionResult();

	void setupKernels();
	void createBuffers();

	std::shared_ptr<RTInteropTexture2D> m_frameImage;
	std::unique_ptr<class Framebuffer> m_framebuffer;
	CLWBuffer<RadeonRays::float4> m_weightedRadianceBuffer;
	CLWBuffer<float> m_filterWeightsBuffer;

	CLWBuffer<RadeonRays::float4> m_weightedRadianceBufferAllFilters;
	CLWBuffer<float> m_filterWeightsBufferAllFilters;

	CLWBuffer<RTFilterProperties> m_filterProperties;
	RTKernel m_reconstructionKernel;
	RTKernel m_reconstructionAllFiltersKernel;
	RTKernel m_copyReconstructionResultKernel;
};