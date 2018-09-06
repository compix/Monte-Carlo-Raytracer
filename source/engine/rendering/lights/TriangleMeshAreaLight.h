#pragma once
#include <engine/ecs/ECS.h>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <imgui/imgui.h>

class TriangleMeshAreaLight : public Component
{
public:
	TriangleMeshAreaLight() { }

	void onShowInEditor() override
	{
		ImGui::ColorEdit3("Color", &color[0]);
		ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 30.0f);
		ImGui::InputInt("Mesh Index", &meshIdx);
	}

	std::string getName() const override { return "Triangle Mesh Area Light"; }

	glm::vec3 color{ 1.0f };
	float intensity{ 1.0f };
	int meshIdx{0};
};
