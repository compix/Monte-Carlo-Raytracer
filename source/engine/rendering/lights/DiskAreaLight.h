#pragma once
#include <engine/ecs/ECS.h>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <imgui/imgui.h>

class DiskAreaLight : public Component
{
public:
	DiskAreaLight() { }

	void onShowInEditor() override
	{
		ImGui::ColorEdit3("Color", &color[0]);
		ImGui::DragFloat("Intensity", &intensity, 0.01f, 0.0f, 30.0f);
		ImGui::DragFloat("Radius", &radius, 0.01f, 0.0f, 30.0f);
	}

	std::string getName() const override { return "Disk Area Light"; }

	glm::vec3 color{ 1.0f };
	float intensity{1.0f};
	float radius{0.5f};
};
