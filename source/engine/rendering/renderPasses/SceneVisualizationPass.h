#pragma once
#include <engine/rendering/shader/Shader.h>
#include <engine/rendering/Framebuffer.h>
#include <engine/rendering/architecture/RenderPass.h>
#include "engine/input/Input.h"
#include "../renderer/SimpleMeshRenderer.h"

/**
* Renders the scene into G-Buffers for deferred shading.
*/
class SceneVisualizationPass : public RenderPass
{
public:
	SceneVisualizationPass();

	void update() override;
private:
	std::shared_ptr<Shader> m_shader;
	std::shared_ptr<SimpleMeshRenderer> m_fullscreenQuadRenderer;
};
