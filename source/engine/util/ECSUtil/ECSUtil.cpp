#include "ECSUtil.h"
#include <engine/ecs/ecs.h>
#include <engine/resource/ResourceManager.h>
#include <engine/util/math.h>
#include <engine/rendering/Material.h>
#include <engine/rendering/renderer/MeshRenderer.h>
#include <engine/util/util.h>
#include "engine/rendering/lights/DirectionalLight.h"
#include "engine/rendering/voxelConeTracing/Globals.h"
#include "engine/rendering/util/GLUtil.h"
#include "future"
#include "../Timer.h"

std::vector<ECSUtil::EntityStreamingState> ECSUtil::m_streamingStates;

void setTextures(const std::string& textureName, const std::string& texturesDirectory,
                 const std::vector<std::string>& texturePaths, Material* material, Texture2DSettings settings)
{
    for (size_t i = 0; i < texturePaths.size(); ++i)
    {
        auto texture = ResourceManager::getTexture(texturesDirectory + texturePaths[i], false, settings);

        if (texture->isValid())
            material->setTexture2D(textureName + std::to_string(i), *texture);
    }
}

void ECSUtil::update()
{
	if (m_streamingStates.size() == 0)
		return;

	// Update entity creation
	Timer timer;
	timer.start();

	while (timer.totalTimeInMicroseconds() < ENTITY_STREAMING_TICK_MICROSECOND)
	{
		while (m_streamingStates.size() > 0 && m_streamingStates[0].modelsToProcess.size() == 0)
		{
			m_streamingStates.erase(m_streamingStates.begin());
			ResourceManager::addLoadingTasks(-1);
		}

		if (m_streamingStates.size() > 0)
		{
			updateEntityStreamingState(m_streamingStates[0]);
		}
		else
		{
			return;
		}

		timer.tick();
	}
}

void ECSUtil::updateEntityStreamingState(EntityStreamingState& state)
{
	auto modelParentPair = state.modelsToProcess[state.modelsToProcess.size() - 1];
	state.modelsToProcess.pop_back();
	Entity entity = createEntity(modelParentPair.model, state.shader, state.importSettings, modelParentPair.parent);
	ComponentPtr<Transform> transform = entity.getComponent<Transform>();
	state.lastProcessed = transform;

	for (auto& child : modelParentPair.model->children)
	{
		ModelParentPair pair;
		pair.model = child.get();
		pair.parent = transform;

		state.modelsToProcess.push_back(pair);
	}

	state.numberOfModelsProcessed++;
	float progress = static_cast<float>(state.numberOfModelsProcessed) / state.totalNumberOfModels;
	ResourceManager::progress = progress;
}

Entity ECSUtil::createEntity(Model* model, std::shared_ptr<Shader> shader, const ModelImportSettings& importSettings, ComponentPtr<Transform> parent)
{
	Entity entity = ECS::createEntity(model->name);
	entity.addComponent<Transform>();
	auto transform = entity.getComponent<Transform>();
	transform->setLocalPosition(model->position);
	transform->setLocalRotation(model->rotation);
	transform->setLocalScale(model->scale);

	if (parent)
		transform->setParent(parent);

	if (model->subMeshes.size() > 0)
	{
		entity.addComponent<MeshRenderer>();

		std::shared_ptr<Mesh> mesh = ResourceManager::getMesh(model);

		transform->setBBox(util::computeBBox(*mesh.get()));

		auto meshRenderer = entity.getComponent<MeshRenderer>();
		meshRenderer->setMesh(mesh);

		for (auto& materialDesc : model->materials)
		{
			auto material = std::make_shared<Material>(shader);
			material->setFloat("u_hasDiffuseTexture", materialDesc.diffuseTextures.size() > 0);
			material->setFloat("u_hasNormalMap", materialDesc.normalTextures.size() > 0);
			material->setFloat("u_hasSpecularMap", materialDesc.specularTextures.size() > 0);
			material->setFloat("u_hasEmissionMap", materialDesc.emissionTextures.size() > 0);
			material->setFloat("u_hasOpacityMap", materialDesc.opacityTextures.size() > 0);
			material->setFloat("u_shininess", materialDesc.shininess);
			material->setColor("u_color", glm::vec4(materialDesc.diffuseColor, 1.0f));
			material->setColor("u_emissionColor", materialDesc.emissiveColor);
			material->setColor("u_specularColor", materialDesc.specularColor);

			setTextures("u_diffuseTexture", importSettings.texturesDirectory, materialDesc.diffuseTextures, material.get(), Texture2DSettings::S_T_REPEAT_ANISOTROPIC);
			setTextures("u_normalMap", importSettings.texturesDirectory, materialDesc.normalTextures, material.get(), Texture2DSettings::S_T_REPEAT_ANISOTROPIC);
			setTextures("u_specularMap", importSettings.texturesDirectory, materialDesc.specularTextures, material.get(), Texture2DSettings::S_T_REPEAT_MIN_MIPMAP_LINEAR_MAG_LINEAR);
			setTextures("u_emissionMap", importSettings.texturesDirectory, materialDesc.emissionTextures, material.get(), Texture2DSettings::S_T_REPEAT_ANISOTROPIC);
			setTextures("u_opacityMap", importSettings.texturesDirectory, materialDesc.opacityTextures, material.get(), Texture2DSettings::S_T_REPEAT_MIN_MAG_NEAREST);

			meshRenderer->addMaterial(material);
		}
	}

	return entity;
}

ComponentPtr<Transform> ECSUtil::startStreamingEntitiesFromModel(Model* model, std::shared_ptr<Shader> shader, const ModelImportSettings& importSettings)
{
	ResourceManager::addLoadingTasks(1);
	m_streamingStates.push_back(EntityStreamingState());
	EntityStreamingState& state = m_streamingStates[0];
	state.importSettings = importSettings;
	state.modelsToProcess.push_back(ModelParentPair(model, ComponentPtr<Transform>()));
	state.shader = shader;
	state.totalNumberOfModels = 1 + model->getNumberOfChildren();

	updateEntityStreamingState(state);
	return state.lastProcessed;
}

ComponentPtr<Transform> ECSUtil::createEntitiesFromModel(Model* model, std::shared_ptr<Shader> shader, 
	const ModelImportSettings& importSettings, ComponentPtr<Transform> parent)
{
    // Create an entity
	Entity entity = createEntity(model, shader, importSettings, parent);
	auto transform = entity.getComponent<Transform>();

    for (auto& child : model->children)
    {
        createEntitiesFromModel(child.get(), shader, importSettings, transform);
    }

    return transform;
}

ComponentPtr<Transform> ECSUtil::createEntitiesFromModel(const std::string& path, std::shared_ptr<Shader> shader, const ModelImportSettings& importSettings)
{
    auto model = ResourceManager::getModel(path, importSettings);
    if (!model)
        return ComponentPtr<Transform>();

    return createEntitiesFromModel(model.get(), shader, importSettings);
}

std::shared_ptr<AsyncFuture<ComponentPtr<Transform>>> ECSUtil::createEntitiesFromModelAsync(const std::string& path, std::shared_ptr<Shader> shader, const ModelImportSettings& importSettings)
{
	auto future = std::make_shared<AsyncFuture<ComponentPtr<Transform>>>();

	std::async(std::launch::async, [path, importSettings, future, shader]() {
		auto modelFuture = ResourceManager::getModelAsync(path, importSettings);
		modelFuture->addOnCompletionListener([future, shader, importSettings](std::shared_ptr<Model> model) {
			auto rootTransform = createEntitiesFromModel(model.get(), shader, importSettings);
			if (rootTransform)
				future->onCompleted(rootTransform);
			else
				future->setState(ProgressState::Failed);
		});
	});

	return future;
}

void ECSUtil::renderEntities(Shader* shader)
{
    for (auto e : ECS::getEntitiesWithComponents<Transform, MeshRenderer>())
        renderEntity(e, shader);
}

void ECSUtil::renderEntities(const std::vector<Entity>& entities, Shader* shader)
{
    for (auto& e : entities)
        renderEntity(e, shader);
}

void ECSUtil::renderEntity(Entity entity, Shader* shader)
{
    auto transform = entity.getComponent<Transform>();
    auto renderer = entity.getComponent<MeshRenderer>();
    assert(transform && renderer);

    shader->setMatrix("u_model", transform->getLocalToWorldMatrix());
    shader->setMatrix("u_modelIT", glm::transpose(glm::inverse(transform->getLocalToWorldMatrix())));

    shader->setUnsignedInt("u_entityID", entity.getID());
    shader->setUnsignedInt("u_entityVersion", entity.getVersion());

    renderer->render(shader);
}

void ECSUtil::renderEntitiesInAABB(const BBox& bbox, Shader* shader)
{
    for (Entity e : ECS::getEntitiesWithComponents<Transform, MeshRenderer>())
    {
        auto transform = e.getComponent<Transform>();

        if (bbox.overlaps(transform->getBBox()))
            renderEntity(e, shader);
    }
}

void ECSUtil::setDirectionalLightUniforms(Shader* shader, GLint shadowMapStartTextureUnit, float pcfRadius)
{
    assert(GL::isShaderBound(shader->getProgram()));

    int lightCount = 0;
    for (auto dirLight : ECS::getEntitiesWithComponents<DirectionalLight, Transform>())
    {
        if (lightCount == MAX_DIR_LIGHT_COUNT)
            break;

        auto dirLightComponent = dirLight.getComponent<DirectionalLight>();
        auto lightTransform = dirLight.getComponent<Transform>();
        std::string arrayIdxStr = "[" + std::to_string(lightCount) + "]";
        std::string dirLightUniformName = "u_directionalLights" + arrayIdxStr;
        std::string shadowUniformName = "u_directionalLightShadowDescs" + arrayIdxStr;

        shader->setVector(dirLightUniformName + ".direction", lightTransform->getForward());
        shader->setVector(dirLightUniformName + ".color", dirLightComponent->color);
        shader->setFloat(dirLightUniformName + ".intensity", dirLightComponent->intensity);

        shader->setInt(shadowUniformName + ".enabled", dirLightComponent->shadowsEnabled ? 1 : 0);

        if (dirLightComponent->shadowsEnabled)
        {
            shader->setMatrix(shadowUniformName + ".view", dirLightComponent->view);
            shader->setMatrix(shadowUniformName + ".proj", dirLightComponent->proj);
            shader->setFloat(shadowUniformName + ".zNear", dirLightComponent->zNear);
            shader->setFloat(shadowUniformName + ".zFar", dirLightComponent->zFar);
            if (pcfRadius < 0.0f)
                pcfRadius = dirLightComponent->pcfRadius;

            shader->setFloat(shadowUniformName + ".pcfRadius", pcfRadius);
            shader->bindTexture2D(dirLightComponent->shadowMap, "u_shadowMaps" + arrayIdxStr, shadowMapStartTextureUnit + lightCount);
        }

        ++lightCount;
    }

    shader->setInt("u_numActiveDirLights", lightCount);
}
