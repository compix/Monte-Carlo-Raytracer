#pragma once
#include "../ecs/EntityManager.h"

class FreeCameraViewController : public Component
{
public:
	FreeCameraViewController();

	virtual void update() override;

	virtual std::string getName() const override;

	float movementSpeed = 1.0f;
	bool bMovedInLastUpdate = false;
private:
};