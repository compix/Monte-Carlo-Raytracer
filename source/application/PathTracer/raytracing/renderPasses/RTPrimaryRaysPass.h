#pragma once
#include <engine/rendering/architecture/RenderPass.h>
#include <radeon_rays.h>
#include <CLW.h>
#include "engine/geometry/Ray.h"
#include "kernel_data.h"
#include "../kernels/RTKernel.h"

class RTPrimaryRaysPass : public RenderPass
{
public:
	RTPrimaryRaysPass();

	virtual void update() override;

	void resize(int width, int height);

private:

	RadeonRays::Buffer* generatePrimaryRays();

	CLWBuffer<RadeonRays::ray> m_rayBuffer;
	CLWBuffer<RadeonRays::Intersection> m_isectBufferCL;
	CLWBuffer<RTRayDifferentials> m_rayDifferentialsBuffer;
	RadeonRays::Buffer* m_isectBuffer;

	RTKernel m_genRaysKernel;

	bool m_hasErrors = false;
};