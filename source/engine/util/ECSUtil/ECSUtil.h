#pragma once
#include <engine/ecs/EntityManager.h>
#include <engine/rendering/shader/Shader.h>
#include <memory>
#include <engine/resource/Model.h>
#include "engine/ecs/ECS.h"
#include "../../resource/AssetImporter.h"
#include "mutex"

#define ENTITY_STREAMING_TICK_MICROSECOND 5000u

class ECSUtil
{
	struct ModelParentPair
	{
		ModelParentPair() {}
		ModelParentPair(Model* model, ComponentPtr<Transform> parent)
			:model(model), parent(parent) {}

		Model* model = nullptr;
		ComponentPtr<Transform> parent;
	};

	struct EntityStreamingState
	{
		ComponentPtr<Transform> lastProcessed;
		ModelImportSettings importSettings;
		std::shared_ptr<Shader> shader;
		std::vector<ModelParentPair> modelsToProcess;
		size_t totalNumberOfModels;
		size_t numberOfModelsProcessed = 0;
	};
public:
	static void update();

	static ComponentPtr<Transform> startStreamingEntitiesFromModel(Model* model, std::shared_ptr<Shader> shader,
		const ModelImportSettings& importSettings);

    static ComponentPtr<Transform> createEntitiesFromModel(Model* model, std::shared_ptr<Shader> shader, 
		const ModelImportSettings& importSettings, ComponentPtr<Transform> parent = ComponentPtr<Transform>());
    static ComponentPtr<Transform> createEntitiesFromModel(const std::string& path, std::shared_ptr<Shader> shader, const ModelImportSettings& importSettings);

	static std::shared_ptr<AsyncFuture<ComponentPtr<Transform>>> createEntitiesFromModelAsync(const std::string& path, std::shared_ptr<Shader> shader,
		const ModelImportSettings& importSettings);

    static void renderEntities(Shader* shader);

    static void renderEntities(const std::vector<Entity>& entities, Shader* shader);

    static void renderEntity(Entity entity, Shader* shader);

    static void renderEntitiesInAABB(const BBox& bbox, Shader* shader);

    template<class... Components>
    static std::vector<Entity> getEntitiesInAABB(const BBox& bbox);

    static void setDirectionalLightUniforms(Shader* shader, GLint shadowMapStartTextureUnit, float pcfRadius = -1.0f);

	static std::vector<EntityStreamingState> m_streamingStates;

private:
	static void updateEntityStreamingState(EntityStreamingState& state);

	static Entity createEntity(Model* model, std::shared_ptr<Shader> shader, const ModelImportSettings& importSettings, ComponentPtr<Transform> parent);
};

template <class ... Components>
std::vector<Entity> ECSUtil::getEntitiesInAABB(const BBox& bbox)
{
    std::vector<Entity> entities;

    for (Entity e : ECS::getEntitiesWithComponents<Components...>())
    {
        auto transform = e.getComponent<Transform>();
        assert(transform);

        if (bbox.overlaps(transform->getBBox()))
            entities.push_back(e);
    }

    return entities;
}
