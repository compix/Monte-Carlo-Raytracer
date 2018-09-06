#pragma once
#include <engine/ecs/ECS.h>
#include <CLW.h>
#include <radeon_rays.h>
#include "../../../../engine/rendering/renderer/MeshRenderer.h"
#include "../../../../engine/rendering/geometry/Mesh.h"
#include "../textures/RTTextures.h"
#include <kernel_data.h>
#include <engine/ecs/ECS.h>
#include <engine/event/event.h>
#include "set"
#include <engine/event/EntityDeactivatedEvent.h>
#include <engine/event/EntityActivatedEvent.h>
#include "../material/RTUberMaterialComponent.h"
#include "../kernels/RTKernel.h"

/**
* Connects host scene with OpenCL.
* Tasks:
* - Fetches static and dynamic scene geometry and commits it to the GPU.
* - Fetches updated geometry and commits it to the GPU.
* - Manages and provides access to surface information required for PBR shading computations.
*
* Notes:
* - RadeonRays intersection api currently doesn't support differentiation between static/dynamic.
*   The functions attachStaticEntities, attachDynamicEntities are thus essentially the same.
*/
class RTScene : public System, public Receiver<ComponentAddedEvent<MeshRenderer>>, 
	public Receiver<EntityDeactivatedEvent>, public Receiver<EntityActivatedEvent>
{
	/**
	* Multiple shape instances share indices and vertices.
	*/
	struct SharedShapeInfo
	{
		uint32_t startIdx;
		uint32_t startVertex;
		uint32_t numTriangles;
		RadeonRays::Shape* instanceTemplateShape;
		int materialId;
		int meshIdx;
	};

	struct RTHostScene
	{
		uint32_t nextIndexStart = 0;
		uint32_t nextVertexStart = 0;
		RadeonRays::Id nextShapeId = 0;
		std::vector<RTShape> shapes;
		std::vector<uint32_t> indices;
		std::vector<RadeonRays::float3> positions;
		std::vector<RadeonRays::float2> uvs;
		std::vector<RadeonRays::float3> normals;
		std::vector<RadeonRays::float3> tangents;
		std::vector<RadeonRays::float3> binormals;
		std::vector<RadeonRays::float3> colors;
		std::vector<RTTextureDesc2D> textures;
		std::vector<unsigned char> textureData;
		std::vector<RTLight> lights;
		std::vector<RTMaterial> materials;
	};
public:
	RTScene(RadeonRays::IntersectionApi* intersectionApi, CLWContext clContext);
	~RTScene();

	virtual void update(EntityManager& entityManager) override;


	virtual void onAppInitialized(EntityManager& entityManager) override;

	const RTDeviceScene& getDeviceScene() const { return m_rtDeviceScene; }

	const CLWBuffer<RTPinholeCamera> getCamera() const { return m_rtDeviceScene.camera; }

	virtual void receive(const ComponentAddedEvent<MeshRenderer>& event) override;


	virtual void receive(const EntityDeactivatedEvent& event) override;


	virtual void receive(const EntityActivatedEvent& event) override;

	void addSceneUpdateListener(std::function<void()> listener) { m_sceneUpdateListeners.push_back(listener); }

	int setSceneArgs(RTKernel& kernel, int sceneArgsStart = 0);

	void commit();

	void refresh();

private:
	void attachStaticEntities();
	void attachDynamicEntities();
	void updateDynamicEntities();
	void attachEntity(Entity entity);

	void addLights();
	void addLight(Entity entity);
	void setLight(Entity entity, RTLight& light, int lightID, RTLightType type);
	bool hasLight(Entity entity);

	void attachMesh(ComponentPtr<MeshRenderer> meshRenderer, RTHostScene& rtHostScene);

	void uploadTextures();
	void uploadShapes();
	void uploadMaterials();
	void uploadLights();
	void computeChoicePdfsForLights();

	RTMaterial createUberMaterial(ComponentPtr<MeshRenderer> meshRenderer, const Material* material, int rtMaterialId);
	void updateRTMaterial(RTMaterial& material, const RTUberMaterialComponent::MaterialData& materialData);
	void updateRTMaterialTextures(RTMaterial& rtMaterial, const Material* material);

	RadeonRays::IntersectionApi* m_intersectionApi = nullptr;
	CLWContext m_clContext;

	RTHostScene m_rtHostScene;
	RTDeviceScene m_rtDeviceScene;

	/** 
	* This map is used to create instances of shapes and share indices + vertices.
	* Meshes are reused per instance so mapping mesh -> shapes accomplishes this goal.
	* A mesh can consist of multiple Mesh::SubMeshes. Each SubMesh is represented by a Shape.
	*/
	std::unordered_map<std::shared_ptr<Mesh>, std::vector<SharedShapeInfo>> m_meshToShapesMap;

	std::unordered_map<std::shared_ptr<Material>, int> m_glMaterialToRTMaterialIdMap;
	std::unordered_map<TextureID, int> m_glTexIdToRTTexId;

	std::vector<Entity> m_scheduledCreatedEntities;
	std::set<Entity> m_attachedEntities;

	std::vector<std::function<void()>> m_sceneUpdateListeners;

	bool m_updated = false;
	BBox m_sceneBBox;
};