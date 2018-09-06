#pragma once
#include "engine/ecs/EntityManager.h"

struct TransformStaticStatusChangedEvent
{
	TransformStaticStatusChangedEvent(const Entity& entity)
		:entity(entity) {}

	Entity entity;
};
