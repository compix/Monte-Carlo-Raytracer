#include "RTDisplayPass.h"
#include "../../../../engine/rendering/renderer/MeshRenderers.h"
#include "../../../../engine/rendering/Texture2D.h"
#include "../../../../engine/resource/ResourceManager.h"
#include <engine/rendering/architecture/RenderPipeline.h>

RTDisplayPass::RTDisplayPass()
	:RenderPass("RTDisplayPass")
{
	// Get fullscreen quad renderer and shader
	m_fullscreenQuadRenderer = MeshRenderers::fullscreenQuad();
	m_fullscreenQuadShader = ResourceManager::getShader("shaders/simple/fullscreenQuad.vert", "shaders/simple/fullscreenQuad.frag");
}

void RTDisplayPass::update()
{
	auto nextFrameImage = m_renderPipeline->fetchPtr<Texture2D>("NextFrameImage");
	if (!nextFrameImage)
		return;

	m_fullscreenQuadShader->bind();
	m_fullscreenQuadShader->bindTexture2D(*nextFrameImage, "u_textureDiffuse");
	m_fullscreenQuadRenderer->bindAndRender();
}