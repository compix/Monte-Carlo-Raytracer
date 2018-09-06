#pragma once
#include <engine/ecs/ECS.h>
#include <glm/glm.hpp>
#include <GL/glew.h>

class PointLight : public Component
{
public:
	PointLight() { }

	PointLight(const glm::vec3& color, float intensity)
		: color(color), intensity(intensity) { }

	void onShowInEditor() override;

	std::string getName() const override { return "Point Light"; }

	glm::vec3 color{ 1.0f };
	float intensity{ 1.0f };
};
