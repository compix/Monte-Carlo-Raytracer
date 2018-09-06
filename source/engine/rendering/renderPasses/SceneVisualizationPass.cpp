#include "SceneVisualizationPass.h"
#include "../../resource/ResourceManager.h"
#include "../architecture/RenderPipeline.h"
#include "../Screen.h"
#include "../util/GLUtil.h"
#include "../settings/RenderingSettings.h"
#include "../renderer/MeshRenderers.h"

SceneVisualizationPass::SceneVisualizationPass()
	:RenderPass("SceneVisualizationPass")
{
	m_shader = ResourceManager::getShader("shaders/sceneVisualizationPass.vert", "shaders/sceneVisualizationPass.frag");
	m_fullscreenQuadRenderer = MeshRenderers::fullscreenQuad();
}

void SceneVisualizationPass::update()
{
	auto camera = m_renderPipeline->getCamera();

	GLuint diffuseTexture = m_renderPipeline->fetch<GLuint>("DiffuseTexture");
	GLuint normalMap = m_renderPipeline->fetch<GLuint>("NormalMap");
	GLuint specularMap = m_renderPipeline->fetch<GLuint>("SpecularMap");
	GLuint emissionMap = m_renderPipeline->fetch<GLuint>("EmissionMap");
	GLuint depthTexture = m_renderPipeline->fetch<GLuint>("DepthTexture");
	GLuint geometryNormalMap = m_renderPipeline->fetch<GLuint>("GeometryNormalMap");
	GLuint uvMap = m_renderPipeline->fetch<GLuint>("UVMap");

	glDisable(GL_DEPTH_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	GL::setViewport(Rect(0.f, 0.f, float(Screen::getWidth()), float(Screen::getHeight())));

	m_shader->bind();
	GLint textureUnit = 0;
	m_shader->bindTexture2D(diffuseTexture, "u_diffuseTexture", textureUnit++);
	m_shader->bindTexture2D(normalMap, "u_normalMap", textureUnit++);
	m_shader->bindTexture2D(specularMap, "u_specularMap", textureUnit++);
	m_shader->bindTexture2D(depthTexture, "u_depthTexture", textureUnit++);
	m_shader->bindTexture2D(emissionMap, "u_emissionMap", textureUnit++);
	m_shader->bindTexture2D(geometryNormalMap, "u_geometryNormalMap", textureUnit++);
	m_shader->bindTexture2D(uvMap, "u_uvMap", textureUnit++);

	m_shader->setMatrix("u_viewProjInv", camera->viewProjInv());
	m_shader->setVector("u_eyePos", camera->getPosition());
	m_shader->setVector("u_cameraDir", camera->getForward());
	m_shader->setInt("u_displayMode", SCENE_VISUALIZATION_SETTINGS.visualizationMode.curItem);
	m_shader->setInt("u_BRDFMode", RENDERING_SETTINGS.brdfMode);
	
	m_fullscreenQuadRenderer->bindAndRender();

	glEnable(GL_DEPTH_TEST);
}
