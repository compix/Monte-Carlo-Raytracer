#pragma once
#include "engine/ecs/EntityManager.h"

struct EntityPreDestroyedEvent
{
	EntityPreDestroyedEvent(const Entity& entity)
		:entity(entity) {}

	Entity entity;
};
