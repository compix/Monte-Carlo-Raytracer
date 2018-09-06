#include "GeometryBufferGUI.h"
#include "GUI.h"
#include "../globals.h"
#include <GL/glew.h>
#include "../rendering/architecture/RenderPipeline.h"

GeometryBufferGUI::GeometryBufferGUI()
{
	window.open = false;
	window.minSize = ImVec2(250, 250);
}

void GeometryBufferGUI::update()
{
	if (window.open)
	{
		window.begin();

		GLuint diffuseTexture = GL_INVALID_INDEX;
		GLuint normalMap = GL_INVALID_INDEX;
		GLuint specularMap = GL_INVALID_INDEX;
		GLuint emissionMap = GL_INVALID_INDEX;
		GLuint depthTexture = GL_INVALID_INDEX;

		CurRenderPipeline->tryFetch<GLuint>("DiffuseTexture", diffuseTexture);
		CurRenderPipeline->tryFetch<GLuint>("NormalMap", normalMap);
		CurRenderPipeline->tryFetch<GLuint>("SpecularMap", specularMap);
		CurRenderPipeline->tryFetch<GLuint>("EmissionMap", emissionMap);
		CurRenderPipeline->tryFetch<GLuint>("DepthTexture", depthTexture);

		if (specularMap != GL_INVALID_INDEX)
		{
			GUI::textures[specularMap] = GUITexture("Specular Map", specularMap, GL_RED, GL_GREEN, GL_BLUE, GL_ONE);
		}

		if (depthTexture != GL_INVALID_INDEX)
		{
			GUI::textures[depthTexture] = GUITexture("Depth Texture", depthTexture, GL_RED, GL_RED, GL_RED, GL_ONE);
		}

		showTextures(window.size, {
			GUITexture("Diffuse Texture", diffuseTexture),
			GUITexture("Normal Map", normalMap),
			GUITexture("Specular Map", specularMap),
			GUITexture("Emission Map", emissionMap),
			GUITexture("Depth Texture", depthTexture) });

		window.end();
	}
}

void GeometryBufferGUI::showTextures(const ImVec2& canvasSize, std::initializer_list<GUITexture> textures) const
{
	size_t count = textures.size();
	size_t nColumns = size_t(ceil(sqrtf(float(count))));
	size_t nRows = size_t(ceil(float(count) / nColumns));

	auto panelSize = ImVec2(canvasSize.x / nColumns, canvasSize.y / nRows - 25.0f);
	auto textureSize = ImVec2(canvasSize.x / nColumns, canvasSize.y / nRows - 50.0f);

	size_t i = 0;

	for (auto& tex : textures)
	{
		if (tex.texID == GL_INVALID_INDEX)
			continue;

		if (i % nColumns > 0)
			ImGui::SameLine();

		ImGui::BeginChild(tex.label.c_str(), panelSize);
		ImGui::Text(tex.label.c_str());
		ImGui::Image(ImTextureID(uintptr_t(tex.texID)), textureSize, ImVec2(0.0f, 1.0f), ImVec2(1.0f, 0.0f));
		ImGui::EndChild();

		++i;
	}
}

