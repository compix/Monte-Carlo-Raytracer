#pragma once
#include "GUIElements.h"
#include "GUITexture.h"
#include "initializer_list"

class GeometryBufferGUI
{
public:
	GeometryBufferGUI();

	void update();

	GUIWindow window = GUIWindow("G-Buffers");
private:
	void showTextures(const ImVec2& canvasSize, std::initializer_list<GUITexture> textures) const;
};