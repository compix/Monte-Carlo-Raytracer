#pragma once
#include <engine/rendering/architecture/RenderPass.h>
#include <radeon_rays.h>
#include <CLW.h>
#include "engine/geometry/Ray.h"
#include "kernel_data.h"
#include "../kernels/RTKernel.h"
#include "../../../../engine/rendering/renderer/SimpleMeshRenderer.h"
#include "../../../../engine/rendering/shader/Shader.h"

class RTDisplayPass : public RenderPass
{
public:
	RTDisplayPass();

	virtual void update() override;

private:
	std::shared_ptr<SimpleMeshRenderer> m_fullscreenQuadRenderer;
	std::shared_ptr<Shader> m_fullscreenQuadShader;
};