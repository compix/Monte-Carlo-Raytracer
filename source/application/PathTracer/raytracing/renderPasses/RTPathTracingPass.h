#pragma once
#include "engine/rendering/architecture/RenderPass.h"
#include <CLW.h>
#include <radeon_rays.h>
#include "engine/ecs/ECS.h"
#include "../kernels/RTKernel.h"
#include "engine/rendering/renderer/SimpleMeshRenderer.h"
#include "engine/rendering/shader/Shader.h"
#include "engine/rendering/Texture2D.h"
#include "kernel_data.h"
#include "../textures/RTInteropTexture2D.h"

class RTPathTracingPass : public RenderPass
{
public:

	RTPathTracingPass();

	virtual void update() override;

private:
	void applyShading(const CLWBuffer<RadeonRays::Intersection> &isect);
	void applyVisibilityTest();
	void createBuffers();

	RTKernel m_kernel;

	CLWBuffer<RadeonRays::ray> m_shadowRayBuffer;
	CLWBuffer<int> m_shadowRayOcclusionBufferCL;
	RadeonRays::Buffer* m_shadowRayOcclusionBuffer;

	int m_bounceCounter = 0;
	CLWBuffer<RadeonRays::float4> m_radianceBuffer;
	CLWBuffer<RadeonRays::float4> m_tempRadianceBuffer;
	CLWBuffer<RTThroughput> m_throughputBuffer;
	RTKernel m_shadowKernel;

	int m_frameIndex = 0;
	bool m_hasErrors = false;
	float m_totalRenderTime = 0.0f;
};