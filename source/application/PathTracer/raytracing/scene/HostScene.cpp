#include "HostScene.h"
#include "engine/util/Logger.h"
#include "algorithm"
#include "../source/engine/util/math.h"

EntityCollection HostScene::getStaticEntities() const
{
	EntityCollection entities;
	for (Entity entity : ECS::getEntitiesWithComponents<Transform>())
	{
		auto transform = entity.getComponent<Transform>();
		if (transform->isStatic())
		{
			entities.push_back(entity);
		}
	}

	return entities;
}

EntityCollection HostScene::getDynamicEntities() const
{
	EntityCollection entities;
	for (Entity entity : ECS::getEntitiesWithComponents<Transform>())
	{
		auto transform = entity.getComponent<Transform>();
		if (!transform->isStatic())
		{
			entities.push_back(entity);
		}
	}

	return entities;
}

void HostScene::update(EntityManager& entityManager)
{
	m_updatedEntities.clear();

	for (Entity entity : entityManager.getEntitiesWithComponents<Transform>())
	{
		auto transform = entity.getComponent<Transform>();

		if (transform->hasChangedSinceLastFrame() && !transform->isStatic())
		{
			glm::vec3 scale = transform->getLocalScale();;
			bool hasZeroScale = math::nearEq(scale.x, 0.0f) || math::nearEq(scale.y, 0.0f) || math::nearEq(scale.z, 0.0f);
			if (!hasZeroScale)
			{
				m_updatedEntities.push_back(entity);
			}
		}
	}
}

void HostScene::onAppInitialized(EntityManager& entityManager)
{
}
