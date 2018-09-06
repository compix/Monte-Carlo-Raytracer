#pragma once
#include <engine/ecs/EntityManager.h>
#include <radeon_rays.h>

class RTShapeComponent : public Component
{
public:

	virtual std::string getName() const override { return "RTShapeComponent"; }

	std::vector<RadeonRays::Shape*> shapes;

	virtual void onShowInEditor() override;

};