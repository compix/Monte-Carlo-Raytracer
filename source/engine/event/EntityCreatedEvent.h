#pragma once
#include "engine/ecs/EntityManager.h"

struct EntityCreatedEvent
{
	EntityCreatedEvent(const Entity& entity)
		:entity(entity) {}

	Entity entity;
};
