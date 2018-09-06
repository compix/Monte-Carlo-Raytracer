#pragma once
#include <engine/rendering/architecture/RenderPass.h>
#include <radeon_rays.h>
#include <CLW.h>
#include "engine/geometry/Ray.h"
#include "kernel_data.h"
#include "../kernels/RTKernel.h"
#include "../../../../engine/rendering/Texture2D.h"
#include "../textures/RTInteropTexture2D.h"

class RTDenoisePass : public RenderPass
{
public:
	RTDenoisePass();

	virtual void update() override;

protected:
	void setupKernels();

	std::shared_ptr<RTInteropTexture2D> m_denoisedImage;
	RTKernel m_bilateralDenoiseKernel;
};