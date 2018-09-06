#include "RTScene.h"
#include "../../../../engine/rendering/renderer/MeshRenderer.h"
#include "../util/CLHelper.h"
#include "../../../../engine/util/Random.h"
#include "HostScene.h"
#include "RTShapeComponent.h"
#include "../source/engine/resource/ResourceManager.h"
#include "../source/engine/util/NamingConvention.h"
#include "../sampling/sobol.h"
#include "../source/engine/util/colors.h"
#include "../rt_globals.h"
#include <engine/rendering/lights/DirectionalLight.h>
#include <engine/rendering/lights/PointLight.h>
#include "../lights/RTLightComponent.h"
#include "../source/engine/rendering/lights/DiskAreaLight.h"
#include "../source/engine/rendering/lights/TriangleMeshAreaLight.h"
#include "../material/RTUberMaterialComponent.h"
#include "../system/RTBufferManager.h"
#include "../source/engine/rendering/Screen.h"
#include "../../GUI/PathTracingSettings.h"
#include "../third_party/RadeonRays/Calc/inc/except.h"
#include "../system/PlatformManager.h"
#include "../third_party/RadeonRays/RadeonRays/include/radeon_rays_cl.h"
#include "../source/engine/util/math.h"

#define RT_SCENE_MEMORY_RECORD_CONTEXT_NAME std::string("RT_SCENE_MEMORY_RECORD_CONTEXT")

RTScene::RTScene(RadeonRays::IntersectionApi* intersectionApi, CLWContext clContext)
	:m_intersectionApi(intersectionApi), m_clContext(clContext)
{
	PathTracerSettings::INTERSECTION_API.refreshApiButton.onButtonClick = [this]() { commit(); };

	try
	{
		uploadTextures();

		attachStaticEntities();
		attachDynamicEntities();
		addLights();

		commit();

		uploadShapes();
		uploadLights();
		uploadMaterials();
	}
	catch (std::exception e)
	{
		LOG_ERROR(e.what());
	}

	RTScopedMemoryRecord memRecord(RT_SCENE_MEMORY_RECORD_CONTEXT_NAME);

	m_rtDeviceScene.camera = RTBufferManager::createBuffer<RTPinholeCamera>(CL_MEM_READ_ONLY, 1);
	m_rtDeviceScene.sobolMatrices = RTBufferManager::createBuffer<uint32_t>(CL_MEM_READ_ONLY, SOBOL_MATRIX_SIZE * SOBOL_NUM_DIMENSIONS, (void*)&g_SobolMatrices32);
}

RTScene::~RTScene()
{
	m_intersectionApi->DetachAll();
	m_intersectionApi->ResetIdCounter();
}

void RTScene::update(EntityManager& entityManager)
{
	m_updated = false;

	if (!ResourceManager::isLoading())
	{
		if (m_scheduledCreatedEntities.size() > 0)
		{
			uploadTextures();

			for (auto entity : m_scheduledCreatedEntities)
			{
				attachEntity(entity);
			}

			m_scheduledCreatedEntities.clear();

			addLights();
			uploadLights();
			uploadMaterials();
			uploadShapes();
			m_updated = true;
		}

		updateDynamicEntities();
	}

	if (m_updated)
	{
		for (auto& l : m_sceneUpdateListeners)
		{
			l();
		}
	}
}

void RTScene::onAppInitialized(EntityManager& entityManager)
{

}

void RTScene::receive(const ComponentAddedEvent<MeshRenderer>& event)
{
	m_scheduledCreatedEntities.push_back(event.entity);
}

void RTScene::receive(const EntityDeactivatedEvent& event)
{
	Entity entity = event.entity;
	bool requestIsectApiCommit = false;
	if (m_attachedEntities.find(entity) != m_attachedEntities.end())
	{
		auto shapeComponent = entity.getComponent<RTShapeComponent>();
		if (shapeComponent)
		{
			requestIsectApiCommit = true;

			for (auto shape : shapeComponent->shapes)
			{
				m_intersectionApi->DetachShape(shape);
			}
		}
	}

	auto light = entity.getComponent<RTLightComponent>();
	if (light)
	{
		addLights();
		uploadLights();
	}

	if (requestIsectApiCommit)
		m_intersectionApi->Commit();

	for (auto& l : m_sceneUpdateListeners)
	{
		l();
	}
}

void RTScene::receive(const EntityActivatedEvent& event)
{
	Entity entity = event.entity;
	bool requestIsectApiCommit = false;

	if (m_attachedEntities.find(entity) != m_attachedEntities.end())
	{
		auto shapeComponent = entity.getComponent<RTShapeComponent>();
		if (shapeComponent)
		{
			requestIsectApiCommit = true;
			for (auto shape : shapeComponent->shapes)
			{
				m_intersectionApi->AttachShape(shape);
			}
		}
	}

	auto light = entity.getComponent<RTLightComponent>();
	if (light)
	{
		addLights();
		uploadLights();
	}

	if (requestIsectApiCommit)
		m_intersectionApi->Commit();

	for (auto& l : m_sceneUpdateListeners)
	{
		l();
	}
}

int RTScene::setSceneArgs(RTKernel& kernel, int sceneArgsStart /*= 0*/)
{
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.shapes);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.indices);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.positions);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.uvs);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.normals);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.tangents);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.binormals);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.colors);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.textures);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.textureData);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.sobolMatrices);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.lights);
	kernel.setArg(sceneArgsStart++, static_cast<int>(m_rtDeviceScene.lights.GetElementCount()));
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.materials);
	kernel.setArg(sceneArgsStart++, m_rtDeviceScene.camera);

	return sceneArgsStart;
}

void RTScene::commit()
{
	auto& settings = PathTracerSettings::INTERSECTION_API;

	m_intersectionApi->SetOption("bvh.force2level", settings.bvh.force2Level);
	m_intersectionApi->SetOption("bvh.forceflat", settings.bvh.forceFlat);

	switch (settings.bvh.accelerationStructure.getEnumValue())
	{
	case ERTAccelerationStructureType::BVH:
		m_intersectionApi->SetOption("acc.type", "bvh");
		break;
	case ERTAccelerationStructureType::FatBVH:
		m_intersectionApi->SetOption("acc.type", "fatbvh");
		break;
	case ERTAccelerationStructureType::HLBVH:
		m_intersectionApi->SetOption("acc.type", "hlbvh");
		break;
	default:
		break;
	}

	switch (settings.bvh.builder.getEnumValue())
	{
	case ERTBVHBuilderType::SAH:
		m_intersectionApi->SetOption("bvh.builder", "sah");
		break;
	case ERTBVHBuilderType::Median:
		m_intersectionApi->SetOption("bvh.builder", "median");
		break;
	default:
		break;
	}

	m_intersectionApi->SetOption("bvh.sah.traversal_cost", settings.bvh.traversalCost);
	m_intersectionApi->SetOption("bvh.sah.num_bins", static_cast<float>(settings.bvh.numBins));
	m_intersectionApi->SetOption("bvh.sah.use_splits", settings.bvh.useSplits);
	m_intersectionApi->SetOption("bvh.sah.max_split_depth", static_cast<float>(settings.bvh.maxSplitDepth));
	m_intersectionApi->SetOption("bvh.sah.min_overlap", settings.bvh.minOverlap);
	m_intersectionApi->SetOption("bvh.sah.extra_node_budget", settings.bvh.extraNodeBudget);

	bool commitFailed = false;

	try
	{
		m_intersectionApi->Commit();
	}
	catch (const std::exception& e)
	{
		Screen::showMessageBox("RTScene: Critical Error. ",
			"There was a critical error in RTScene. Please check the console for more info.");
		LOG_ERROR(e.what());
		commitFailed = true;
	}
	catch (const Calc::Exception& e)
	{
		Screen::showMessageBox("RTScene: Critical Error. ",
			"There was a critical error in RTScene. Please check the console for more info.");
		LOG_ERROR(e.what());
		commitFailed = true;
	}

	if (commitFailed && settings.bvh.accelerationStructure == ERTAccelerationStructureType::HLBVH)
	{
		LOG("It is possible that the Radeon Rays hlbvh implemenation fails on some platforms. Switching to default BVH...");
		settings.bvh.accelerationStructure.curItem = static_cast<int>(ERTAccelerationStructureType::BVH);
		commit();
	}
}

void RTScene::refresh()
{
	if (ResourceManager::isLoading())
		return;

	uploadTextures();

	attachStaticEntities();
	attachDynamicEntities();
	addLights();

	for (auto& pair : m_glMaterialToRTMaterialIdMap)
	{
		updateRTMaterialTextures(m_rtHostScene.materials[pair.second], pair.first.get());
	}

	uploadShapes();
	uploadLights();
	uploadMaterials();

	for (auto& l : m_sceneUpdateListeners)
	{
		l();
	}
}

void RTScene::attachStaticEntities()
{
	auto hostScene = ECS::getSystem<HostScene>();
	assert(hostScene);

	for (auto entity : hostScene->getStaticEntities())
	{
		attachEntity(entity);
	}
}

void RTScene::attachDynamicEntities()
{
	auto hostScene = ECS::getSystem<HostScene>();
	assert(hostScene);

	for (auto entity : hostScene->getDynamicEntities())
	{
		attachEntity(entity);
	}
}

void RTScene::updateDynamicEntities()
{
	auto hostScene = ECS::getSystem<HostScene>();
	assert(hostScene);
	bool updatedLights = false;

	if (hostScene->getUpdatedEntities().size() > 0)
	{
		for (auto entity : hostScene->getUpdatedEntities())
		{
			auto shapeComponent = entity.getComponent<RTShapeComponent>();
			if (shapeComponent)
			{
				auto transform = entity.getComponent<Transform>();
				for (auto shape : shapeComponent->shapes)
				{
					shape->SetTransform(CLHelper::toMatrix(transform->getLocalToWorldMatrix()),
						CLHelper::toMatrix(transform->getWorldToLocalMatrix()));

					m_rtHostScene.shapes[shape->GetId()].toWorldTransform = CLHelper::toMatrix(transform->getLocalToWorldMatrix());
					m_rtHostScene.shapes[shape->GetId()].toWorldInverseTranspose = 
						CLHelper::toMatrix(glm::transpose(glm::inverse(transform->getLocalToWorldMatrix())));
				}
			}

			auto lightComp = entity.getComponent<RTLightComponent>();
			if (lightComp)
			{
				for (auto& p : lightComp->hostLightIndexMap)
				{
					if (p.second >= 0)
					{
						updatedLights = true;
						setLight(entity, m_rtHostScene.lights[p.second], p.second, p.first);
					}
				}
			}
		}

		auto writeEvt = m_clContext.WriteBuffer(0, m_rtDeviceScene.shapes, m_rtHostScene.shapes.data(), m_rtHostScene.shapes.size());
		writeEvt.Wait();

		m_intersectionApi->Commit();

		if (updatedLights)
		{
			writeEvt = m_clContext.WriteBuffer(0, m_rtDeviceScene.lights, m_rtHostScene.lights.data(), m_rtHostScene.lights.size());
			writeEvt.Wait();
		}
		
		m_updated = true;
	}

	bool updatedMaterials = false;
	for (auto entity : ECS::getEntitiesWithComponents<RTUberMaterialComponent>())
	{
		auto uberMaterial = entity.getComponent<RTUberMaterialComponent>();
		for (auto& mat : uberMaterial->materialData)
		{
			if (mat.updated)
			{
				updateRTMaterial(m_rtHostScene.materials[mat.rtMaterialId], mat);
				mat.updated = false;
				updatedMaterials = true;
			}
		}
	}

	if (updatedMaterials)
	{
		auto writeEvt = m_clContext.WriteBuffer(0, m_rtDeviceScene.materials, m_rtHostScene.materials.data(), m_rtHostScene.materials.size());
		writeEvt.Wait();
		m_updated = true;
	}
}

void RTScene::attachEntity(Entity entity)
{
	if (m_attachedEntities.find(entity) != m_attachedEntities.end())
		return;

	auto meshRenderer = entity.getComponent<MeshRenderer>();
	auto transform = entity.getComponent<Transform>();

	if (meshRenderer && transform)
	{
		auto shapeComponent = entity.getComponent<RTShapeComponent>();
		if (!shapeComponent)
			entity.addComponent<RTShapeComponent>();

		m_attachedEntities.insert(entity);
		attachMesh(meshRenderer, m_rtHostScene);
	}
}

void RTScene::addLights()
{
	m_rtHostScene.lights.clear();

	for (auto entity : ECS::getEntitiesWithComponents<Transform>())
	{
		if (entity.isActive())
			addLight(entity);
	}

	computeChoicePdfsForLights();
}

void RTScene::addLight(Entity entity)
{
	auto dirLight = entity.getComponent<DirectionalLight>();
	auto pointLight = entity.getComponent<PointLight>();
	auto diskAreaLight = entity.getComponent<DiskAreaLight>();
	auto triMeshAreaLight = entity.getComponent<TriangleMeshAreaLight>();
	auto transform = entity.getComponent<Transform>();

	if (dirLight)
	{
		if (!entity.getComponent<RTLightComponent>())
			entity.addComponent<RTLightComponent>();
		auto rtLightComp = entity.getComponent<RTLightComponent>();
		RTLight light;
		setLight(entity, light, static_cast<int>(m_rtHostScene.lights.size()), RT_DIRECTIONAL_LIGHT);
		m_rtHostScene.lights.push_back(light);
		rtLightComp->hostLightIndexMap[RT_DIRECTIONAL_LIGHT] = static_cast<int>(m_rtHostScene.lights.size() - 1);
	}

	if (pointLight)
	{
		if (!entity.getComponent<RTLightComponent>())
			entity.addComponent<RTLightComponent>();
		auto rtLightComp = entity.getComponent<RTLightComponent>();
		RTLight light;
		setLight(entity, light, static_cast<int>(m_rtHostScene.lights.size()), RT_POINT_LIGHT);
		m_rtHostScene.lights.push_back(light);
		rtLightComp->hostLightIndexMap[RT_POINT_LIGHT] = static_cast<int>(m_rtHostScene.lights.size() - 1);
	}

	if (diskAreaLight)
	{
		if (!entity.getComponent<RTLightComponent>())
			entity.addComponent<RTLightComponent>();
		auto rtLightComp = entity.getComponent<RTLightComponent>();
		RTLight light;
		setLight(entity, light, static_cast<int>(m_rtHostScene.lights.size()), RT_DISK_AREA_LIGHT);
		m_rtHostScene.lights.push_back(light);
		rtLightComp->hostLightIndexMap[RT_DISK_AREA_LIGHT] = static_cast<int>(m_rtHostScene.lights.size() - 1);
	}
	
	if (triMeshAreaLight)
	{
		if (!entity.getComponent<RTLightComponent>())
			entity.addComponent<RTLightComponent>();
		auto rtLightComp = entity.getComponent<RTLightComponent>();
		RTLight light;
		setLight(entity, light, static_cast<int>(m_rtHostScene.lights.size()), RT_TRIANGLE_MESH_AREA_LIGHT);
		m_rtHostScene.lights.push_back(light);
		rtLightComp->hostLightIndexMap[RT_TRIANGLE_MESH_AREA_LIGHT] = static_cast<int>(m_rtHostScene.lights.size() - 1);
	}
}

void RTScene::setLight(Entity entity, RTLight& light, int lightID, RTLightType type)
{
	auto transform = entity.getComponent<Transform>();

	switch (type)
	{
	case RT_DIRECTIONAL_LIGHT:
	{
		auto dirLight = entity.getComponent<DirectionalLight>();
		light.type = RT_DIRECTIONAL_LIGHT;
		light.d = CLHelper::toFloat3(transform->getForward());
		glm::vec3 I = dirLight->color * dirLight->intensity;
		light.intensity = CLHelper::toFloat3(I);
		light.radius = glm::length(m_sceneBBox.max() - m_sceneBBox.min()) * 0.5f;
		light.p = CLHelper::toFloat3(m_sceneBBox.center() - transform->getForward() * light.radius);
		light.flags = RT_LIGHT_FLAG_DELTA_DIRECTION;
		light.area = math::PI * light.radius * light.radius;
		break;
	}
	case RT_POINT_LIGHT:
	{
		auto pointLight = entity.getComponent<PointLight>();
		light.type = RT_POINT_LIGHT;
		light.flags = RT_LIGHT_FLAG_DELTA_POSITION;
		light.p = CLHelper::toFloat3(transform->getPosition());
		glm::vec3 I = pointLight->color * pointLight->intensity;
		light.intensity = CLHelper::toFloat3(I);
		break;
	}
	case RT_DISK_AREA_LIGHT:
	{
		auto areaLight = entity.getComponent<DiskAreaLight>();
		auto rtShapeComponent = entity.getComponent<RTShapeComponent>();
		light.type = RT_DISK_AREA_LIGHT;
		light.flags = RT_LIGHT_FLAG_AREA;
		light.p = CLHelper::toFloat3(transform->getPosition());
		light.d = CLHelper::toFloat3(transform->getForward());
		glm::vec3 I = areaLight->color * areaLight->intensity;
		light.intensity = CLHelper::toFloat3(I);
		light.radius = areaLight->radius;
		float diskArea = math::PI * light.radius * light.radius;
		light.area = diskArea;

		if (rtShapeComponent && rtShapeComponent->shapes.size() > 0)
		{
			int shapeId = rtShapeComponent->shapes[0]->GetId();
			light.shapeId = shapeId;
			m_rtHostScene.shapes[shapeId].lightID = lightID;
		}
		break;
	}
	case RT_TRIANGLE_MESH_AREA_LIGHT:
	{
		auto areaLight = entity.getComponent<TriangleMeshAreaLight>();
		auto rtShapeComponent = entity.getComponent<RTShapeComponent>();
		auto meshRenderer = entity.getComponent<MeshRenderer>();
		
		if (!rtShapeComponent || areaLight->meshIdx >= rtShapeComponent->shapes.size() || 
			areaLight->meshIdx < 0 || areaLight->meshIdx >= meshRenderer->getMesh()->getSubMeshes().size())
			break;

		light.type = RT_TRIANGLE_MESH_AREA_LIGHT;
		light.flags = RT_LIGHT_FLAG_AREA;
		light.p = CLHelper::toFloat3(transform->getPosition());
		int shapeId = rtShapeComponent->shapes[areaLight->meshIdx]->GetId();
		light.shapeId = shapeId;
		m_rtHostScene.shapes[shapeId].lightID = lightID;
		light.area = meshRenderer->computeSurfaceArea(areaLight->meshIdx);
		glm::vec3 I = areaLight->color * areaLight->intensity;
		light.intensity = CLHelper::toFloat3(I);
		break;
	}
	default:
		break;
	}
}

bool RTScene::hasLight(Entity entity)
{
	auto dirLight = entity.getComponent<DirectionalLight>();
	auto pointLight = entity.getComponent<PointLight>();
	auto diskAreaLight = entity.getComponent<DiskAreaLight>();

	return dirLight || pointLight || diskAreaLight;
}

void RTScene::attachMesh(ComponentPtr<MeshRenderer> meshRenderer, RTHostScene& rtHostScene)
{
	assert(meshRenderer);

	auto mesh = meshRenderer->getMesh();
	auto transform = meshRenderer->getComponent<Transform>();
	auto rtShapeComponent = meshRenderer->getComponent<RTShapeComponent>();
	auto sharedShapesInfoItr = m_meshToShapesMap.find(mesh);

	m_sceneBBox.unite(transform->getBBox());

	if (sharedShapesInfoItr != m_meshToShapesMap.end())
	{
		for (RTScene::SharedShapeInfo& sharedShapeInfo : sharedShapesInfoItr->second)
		{
			auto shapeInst = m_intersectionApi->CreateInstance(sharedShapeInfo.instanceTemplateShape);
	
			rtHostScene.shapes.emplace_back(RTShape());
			auto& rtShape = rtHostScene.shapes[rtHostScene.shapes.size() - 1];
			rtShape.startIdx = sharedShapeInfo.startIdx;
			rtShape.startVertex = sharedShapeInfo.startVertex;
			rtShape.toWorldTransform = CLHelper::toMatrix(transform->getLocalToWorldMatrix());
			rtShape.toWorldInverseTranspose =
				CLHelper::toMatrix(glm::transpose(glm::inverse(transform->getLocalToWorldMatrix())));
			rtShape.numTriangles = sharedShapeInfo.numTriangles;
			rtShape.materialId = sharedShapeInfo.materialId;
			rtShape.area = meshRenderer->computeSurfaceArea(sharedShapeInfo.meshIdx);
			m_intersectionApi->AttachShape(shapeInst);
			shapeInst->SetId(rtHostScene.nextShapeId++);
			shapeInst->SetTransform(CLHelper::toMatrix(transform->getLocalToWorldMatrix()), 
				                    CLHelper::toMatrix(transform->getWorldToLocalMatrix()));
			rtShapeComponent->shapes.push_back(shapeInst);
		}
	
		return;
	}

	std::vector<SharedShapeInfo> shapeInfos;
	for (int meshIdx = 0; meshIdx < mesh->getSubMeshes().size(); ++meshIdx)
	{
		auto& subMesh = mesh->getSubMeshes()[meshIdx];
		auto material = meshRenderer->getMaterial(meshIdx);
		SharedShapeInfo sharedShapeInfo;

		auto rtMaterialIdIter = m_glMaterialToRTMaterialIdMap.find(material);
		if (rtMaterialIdIter != m_glMaterialToRTMaterialIdMap.end())
		{
			sharedShapeInfo.materialId = rtMaterialIdIter->second;
		}
		else
		{
			int materialId = static_cast<int>(rtHostScene.materials.size());
			rtHostScene.materials.push_back(createUberMaterial(meshRenderer, material.get(), materialId));
			m_glMaterialToRTMaterialIdMap[material] = materialId;
			sharedShapeInfo.materialId = materialId;
		}

		sharedShapeInfo.startIdx = rtHostScene.nextIndexStart;
		sharedShapeInfo.startVertex = rtHostScene.nextVertexStart;
		sharedShapeInfo.numTriangles = static_cast<uint32_t>(subMesh.indices.size() / 3);
		sharedShapeInfo.meshIdx = meshIdx;

		rtHostScene.shapes.emplace_back(RTShape());
		auto& rtShape = rtHostScene.shapes[rtHostScene.shapes.size() - 1];
		rtShape.startIdx = sharedShapeInfo.startIdx;
		rtShape.startVertex = sharedShapeInfo.startVertex;
		rtShape.toWorldTransform = CLHelper::toMatrix(transform->getLocalToWorldMatrix());
		rtShape.toWorldInverseTranspose =
			CLHelper::toMatrix(glm::transpose(glm::inverse(transform->getLocalToWorldMatrix())));
		rtShape.numTriangles = sharedShapeInfo.numTriangles;
		rtHostScene.nextIndexStart += static_cast<uint32_t>(subMesh.indices.size());
		rtHostScene.nextVertexStart += static_cast<uint32_t>(subMesh.vertices.size());
		rtShape.materialId = sharedShapeInfo.materialId;
		rtShape.area = meshRenderer->computeSurfaceArea(meshIdx);

		rtHostScene.indices.insert(rtHostScene.indices.end(), subMesh.indices.begin(), subMesh.indices.end());

		for (size_t i = 0; i < subMesh.vertices.size(); ++i)
		{
			rtHostScene.positions.push_back(CLHelper::toFloat3(subMesh.vertices[i]));
			rtHostScene.uvs.push_back(CLHelper::toFloat2(subMesh.uvs[i]));
			rtHostScene.normals.push_back(CLHelper::toFloat3(subMesh.normals[i]));
			rtHostScene.tangents.push_back(CLHelper::toFloat3(subMesh.tangents[i]));
			rtHostScene.binormals.push_back(CLHelper::toFloat3(subMesh.bitangents[i]));
		}

		if (subMesh.colors.size() == 0)
		{
			glm::vec3 randColor = Random::getVec3(0.5f, 1.0f);
			for (size_t i = 0; i < subMesh.vertices.size(); ++i)
			{
				rtHostScene.colors.push_back(CLHelper::toFloat3(randColor));
			}
		}
		
		RadeonRays::Shape* shape = m_intersectionApi->CreateMesh(&subMesh.vertices[0].x,
			static_cast<int>(subMesh.vertices.size()), sizeof(glm::vec3), reinterpret_cast<const int*>(subMesh.indices.data()),
			0, nullptr, static_cast<int>(subMesh.indices.size()) / 3);

		assert(shape != nullptr);
		m_intersectionApi->AttachShape(shape);

		sharedShapeInfo.instanceTemplateShape = shape;

		shape->SetId(rtHostScene.nextShapeId++);
		shape->SetTransform(CLHelper::toMatrix(transform->getLocalToWorldMatrix()),
						    CLHelper::toMatrix(transform->getWorldToLocalMatrix()));

		shapeInfos.push_back(sharedShapeInfo);

		rtShapeComponent->shapes.push_back(shape);
	}

	m_meshToShapesMap.emplace(std::make_pair(mesh, shapeInfos));
}

void RTScene::uploadTextures()
{
	// Load textures
	// Compute required memory
	auto textures = ResourceManager::getTextures2D();
	size_t totalTextureMemSize = 0;
	for (auto& tex : textures)
	{
		int w = tex->getWidth();
		int h = tex->getHeight();

		for (int i = 0; i < tex->getNumMipmapLevels(); ++i)
		{
			int channelCount = 4;
			totalTextureMemSize += channelCount * w * h;
			w = std::max(w / 2, 1);
			h = std::max(h / 2, 1);
		}
	}

	if (totalTextureMemSize > 0)
	{
		unsigned char* texData = new unsigned char[totalTextureMemSize];
		uint32_t memOffset = 0;

		int texId = 0;
		for (auto& tex : textures)
		{
			m_glTexIdToRTTexId[tex->getGLID()] = texId;

			RTTextureDesc2D texDesc;
			int channelCount = 4;
			int packAlignmentValue = 4;

			switch (tex->getTextureFormat())
			{
			case GL_LUMINANCE:
			case GL_DEPTH_COMPONENT:
			case GL_RED:
			case GL_R:
				texDesc.format = RT_TEX_FORMAT_R8;
				break;
			case GL_LUMINANCE_ALPHA:
			case GL_RG:
				texDesc.format = RT_TEX_FORMAT_RG8;
				break;
			case GL_RGB:
				texDesc.format = RT_TEX_FORMAT_RGB8;
				break;
			case GL_RGBA:
				texDesc.format = RT_TEX_FORMAT_RGBA8;
				break;
			default: break;
			}

			texDesc.memOffset = memOffset;
			texDesc.width = static_cast<uint16_t>(tex->getWidth());
			texDesc.height = static_cast<uint16_t>(tex->getHeight());
			texDesc.numMipLevels = static_cast<uint16_t>(tex->getNumMipmapLevels());
			texDesc.wrap = RT_TEX_WRAP_REPEAT;

			m_rtHostScene.textures.push_back(texDesc);

			int w = tex->getWidth();
			int h = tex->getHeight();
			tex->bind();
			glPixelStorei(GL_PACK_ALIGNMENT, packAlignmentValue);

			for (int i = 0; i < tex->getNumMipmapLevels(); ++i)
			{
				glGetTexImage(GL_TEXTURE_2D, i, GL_RGBA, GL_UNSIGNED_BYTE, texData + memOffset);
				memOffset += channelCount * w * h;
				w = std::max(w / 2, 1);
				h = std::max(h / 2, 1);
			}
			++texId;
		}

		std::string texMemRecord = RT_SCENE_MEMORY_RECORD_CONTEXT_NAME + "_TEXTURES";
		RTScopedMemoryRecord memRecord(texMemRecord);

		m_rtDeviceScene.textures = RTBufferManager::createBuffer<RTTextureDesc2D>(CL_MEM_READ_ONLY, m_rtHostScene.textures.size(), m_rtHostScene.textures.data());
		m_rtDeviceScene.textureData = RTBufferManager::createBuffer<unsigned char>(CL_MEM_READ_ONLY, totalTextureMemSize, texData);

		delete[] texData;
	}
}

void RTScene::uploadShapes()
{
	if (m_rtHostScene.shapes.size() == 0)
		return;

	std::string shapesMemRecord = RT_SCENE_MEMORY_RECORD_CONTEXT_NAME + "_SHAPES";
	RTScopedMemoryRecord memRecord(shapesMemRecord);

	m_rtDeviceScene.shapes = RTBufferManager::createBuffer<RTShape>(CL_MEM_READ_ONLY, m_rtHostScene.shapes.size(), m_rtHostScene.shapes.data());
	m_rtDeviceScene.indices = RTBufferManager::createBuffer<uint32_t>(CL_MEM_READ_ONLY, m_rtHostScene.indices.size(), m_rtHostScene.indices.data());
	m_rtDeviceScene.positions = RTBufferManager::createBuffer<RadeonRays::float3>(CL_MEM_READ_ONLY, m_rtHostScene.positions.size(), m_rtHostScene.positions.data());
	m_rtDeviceScene.uvs = RTBufferManager::createBuffer<RadeonRays::float2>(CL_MEM_READ_ONLY, m_rtHostScene.uvs.size(), m_rtHostScene.uvs.data());
	m_rtDeviceScene.normals = RTBufferManager::createBuffer<RadeonRays::float3>(CL_MEM_READ_ONLY, m_rtHostScene.normals.size(), m_rtHostScene.normals.data());
	m_rtDeviceScene.tangents = RTBufferManager::createBuffer<RadeonRays::float3>(CL_MEM_READ_ONLY, m_rtHostScene.tangents.size(), m_rtHostScene.tangents.data());
	m_rtDeviceScene.binormals = RTBufferManager::createBuffer<RadeonRays::float3>(CL_MEM_READ_ONLY, m_rtHostScene.binormals.size(), m_rtHostScene.binormals.data());
	m_rtDeviceScene.colors = RTBufferManager::createBuffer<RadeonRays::float3>(CL_MEM_READ_ONLY, m_rtHostScene.colors.size(), m_rtHostScene.colors.data());
}

void RTScene::uploadMaterials()
{
	if (m_rtHostScene.materials.size() == 0)
		return;

	std::string materialsMemRecord = RT_SCENE_MEMORY_RECORD_CONTEXT_NAME + "_MATERIALS";
	RTScopedMemoryRecord memRecord(materialsMemRecord);

	m_rtDeviceScene.materials = RTBufferManager::createBuffer<RTMaterial>(CL_MEM_READ_ONLY, m_rtHostScene.materials.size(), m_rtHostScene.materials.data());
}

void RTScene::uploadLights()
{
	if (m_rtHostScene.lights.size() == 0)
	{
		m_rtDeviceScene.lights = CLWBuffer<RTLight>();
		return;
	}

	std::string lightsMemRecord = RT_SCENE_MEMORY_RECORD_CONTEXT_NAME + "_LIGHTS";
	RTScopedMemoryRecord memRecord(lightsMemRecord);
	
	m_rtDeviceScene.lights = RTBufferManager::createBuffer<RTLight>(CL_MEM_READ_ONLY, m_rtHostScene.lights.size(), m_rtHostScene.lights.data());
}

void RTScene::computeChoicePdfsForLights()
{
	// Set choice pdf to uniform distribution.
	// This can be improved with a distribution based on the power of the light.
	for (auto& light : m_rtHostScene.lights)
	{
		light.choicePdf = 1.0f / m_rtHostScene.lights.size();
	}
}

RTMaterial RTScene::createUberMaterial(ComponentPtr<MeshRenderer> meshRenderer, const Material* material, int rtMaterialId)
{
	RTMaterial rtMaterial;

	auto uberMaterialComp = meshRenderer->getComponent<RTUberMaterialComponent>();
	if (!uberMaterialComp)
	{
		meshRenderer->addComponent<RTUberMaterialComponent>();
		uberMaterialComp = meshRenderer->getComponent<RTUberMaterialComponent>();
	}

	updateRTMaterialTextures(rtMaterial, material);

	uberMaterialComp->materialData.resize(uberMaterialComp->materialData.size() + 1);
	auto& mat = uberMaterialComp->materialData[uberMaterialComp->materialData.size() - 1];

	mat.diffuseColor = material->getColor("u_color");
	mat.glossColor = material->getColor3("u_specularColor");
	float shininess = material->getFloat("u_shininess");
	mat.roughness = glm::vec2(math::clamp(std::sqrt(2.0f / (shininess + 2.0f)), 0.00001f, 1.0f));

	mat.rtMaterialId = rtMaterialId;
	updateRTMaterial(rtMaterial, mat);

	return rtMaterial;
}

void RTScene::updateRTMaterial(RTMaterial& material, const RTUberMaterialComponent::MaterialData& materialData)
{
	material.uber_kd = CLHelper::toFloat3(materialData.diffuseColor);
	material.uber_ks = CLHelper::toFloat3(materialData.glossColor);
	material.uber_kr = CLHelper::toFloat3(materialData.specularReflectionColor);
	material.uber_kt = CLHelper::toFloat4(glm::vec4(materialData.specularTransmissionColor, materialData.isGlossyTransmission ? 1.0f : 0.0f));
	material.uber_opacity = CLHelper::toFloat3(materialData.opacity);
	material.uber_roughness = CLHelper::toFloat2(materialData.roughness);
	material.uber_eta = materialData.eta;
}

void RTScene::updateRTMaterialTextures(RTMaterial& rtMaterial, const Material* material)
{
	TextureID texID;
	if (material->tryGetTexture2D(NC::diffuseTexture(0), texID))
	{
		rtMaterial.uber_diffuseTexId = m_glTexIdToRTTexId[texID];
	}

	if (material->tryGetTexture2D(NC::normalMap(0), texID))
	{
		rtMaterial.uber_normalMapId = m_glTexIdToRTTexId[texID];
	}

	if (material->tryGetTexture2D(NC::opacityMap(0), texID))
	{
		rtMaterial.uber_opacityTexId = m_glTexIdToRTTexId[texID];
	}

	if (material->tryGetTexture2D(NC::specularMap(0), texID))
	{
		rtMaterial.uber_glossyTexId = m_glTexIdToRTTexId[texID];
	}
}
