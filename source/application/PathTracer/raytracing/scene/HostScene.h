#pragma once
#include <engine/ecs/ECS.h>
#include "vector"
#include <engine/geometry/Transform.h>
#include "../source/engine/event/TransformStaticStatusChangedEvent.h"
#include "../source/engine/event/EntityActivatedEvent.h"
#include "../source/engine/event/EntityDeactivatedEvent.h"
#include <set>

using EntityCollection = std::vector<Entity>;

/**
* Responsible for entity transform management:
* - static entities: Entities that won't be transformed.
* - dynamic entities: Entities that can be transformed.
* - updated entities: Entities that were updated in the current frame.
*/
class HostScene : public System
{
public:
	EntityCollection getStaticEntities() const;
	EntityCollection getDynamicEntities() const;
	const EntityCollection& getUpdatedEntities() const { return m_updatedEntities; }

	virtual void update(EntityManager& entityManager) override;

	virtual void onAppInitialized(EntityManager& entityManager) override;

private:
	std::vector<Entity> m_updatedEntities;
};