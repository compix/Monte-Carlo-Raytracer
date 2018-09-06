#include "PointLight.h"
#include <imgui/imgui.h>

void PointLight::onShowInEditor()
{
	ImGui::ColorEdit3("Color", &color[0]);
	ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 20.0f);
}
