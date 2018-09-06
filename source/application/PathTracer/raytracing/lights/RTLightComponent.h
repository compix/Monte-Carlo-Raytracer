#pragma once
#include <engine/ecs/ECS.h>

class RTLightComponent : public Component
{
public:

	RTLightComponent() {}

	virtual std::string getName() const override
	{
		return "RT Light Component";
	}

	std::map<RTLightType, int> hostLightIndexMap;
};
