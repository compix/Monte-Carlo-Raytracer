#include "AssetImporter.h"
#include <engine/util/Logger.h>
#include <vector>
#include <engine/rendering/geometry/Mesh.h>
#include <memory>
#include <engine/util/util.h>

#include <assimp/cimport.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/Importer.hpp>
#include "../util/Timer.h"
#include "../../../third_party/assimp/include/assimp/ProgressHandler.hpp"
#include "../util/math.h"
#include "future"
#include "mutex"
#include "ResourceManager.h"

class AssetImportProgressHandler : public Assimp::ProgressHandler
{
public:

	virtual bool Update(float percentage = -1.f) override
	{
		ResourceManager::progress = math::clamp(percentage, 0.0f, 1.0f);
		return true;
	}
};


std::shared_ptr<Model> AssetImporter::import(const std::string& filename, const ModelImportSettings& importSettings)
{
	return import(filename, importSettings, new AssetImportProgressHandler());
}

std::shared_ptr<Model> AssetImporter::import(const std::string& filename, const ModelImportSettings& importSettings, Assimp::ProgressHandler* progressHandler)
{
	Assimp::Importer importer;
	importer.SetProgressHandler(progressHandler);
	const aiScene* scene = importer.ReadFile(filename, aiProcessPreset_TargetRealtime_Fast | aiProcess_MakeLeftHanded | aiProcess_FlipWindingOrder);

	if (!scene)
	{
		LOG(importer.GetErrorString());
		return nullptr;
	}

	auto model = process(scene, scene->mRootNode);

	if (importSettings.mapToUnitCube)
	{
		model->mapToUnitCube();
	}

	if (importSettings.translationAsModelCenter)
	{
		model->setOriginToModelCenter();
	}

	model->scale = importSettings.scale;

	return model;
}

std::shared_ptr<AsyncFuture<std::shared_ptr<Model>>> AssetImporter::importAsync(const std::string& filename, const ModelImportSettings& settings)
{
	auto modelImportFuture = std::make_shared<AsyncFuture<std::shared_ptr<Model>>>();

	static std::future<void> future;

	future = std::async(std::launch::async, [filename, settings, modelImportFuture]() {
		ResourceManager::addLoadingTasks(1);
		auto model = AssetImporter::import(filename, settings, new ModelImportProgressHandler());
		modelImportFuture->onCompleted(model);
		ResourceManager::addLoadingTasks(-1);
	});

	return modelImportFuture;
}

/**
* Adds all available textures of the given type in meterial to textures.
*/
void addTextures(unsigned type, aiMaterial* material, std::vector<std::string>& textures)
{
    aiString texString;
    int idx = 0;
    while (AI_SUCCESS == material->Get(_AI_MATKEY_TEXTURE_BASE, type, idx, texString))
    {
        // Convert "\\" to "/"
        std::string texture = texString.C_Str();
        std::vector<std::string> splitted = util::split(texture, "\\");
        texture = "";
        for (size_t j = 0; j < splitted.size(); ++j)
            texture += splitted[j] + (j + 1 < splitted.size() ? "/" : "");

        textures.push_back(texture);
        ++idx;
    }
}

void setColor(char* pKey, unsigned type, unsigned idx, aiMaterial* material, const glm::vec3& defaultColor, glm::vec3& colorOut)
{
    aiColor3D color;
    if (AI_SUCCESS == material->Get(pKey, type, idx, color))
        colorOut = glm::vec3(color.r, color.g, color.b);
    else
        colorOut = defaultColor;
}

void setFloat(char* pKey, unsigned type, unsigned idx, aiMaterial* material, float defaultVal, float& vOut)
{
    float val;
    if (AI_SUCCESS == material->Get(pKey, type, idx, val))
        vOut = val;
    else
        vOut = defaultVal;
}

std::shared_ptr<Model> AssetImporter::process(const aiScene* scene, const aiNode* node)
{
    std::shared_ptr<Model> model = std::make_shared<Model>();
    model->name = node->mName.C_Str();

    if (node->mNumMeshes > 0)
    {
        model->subMeshes.resize(node->mNumMeshes);
        model->materials.resize(node->mNumMeshes);

        for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        {
            Mesh::SubMesh& subMesh = model->subMeshes[i];
            MaterialDescription& materialDesc = model->materials[i];

            auto meshIdx = node->mMeshes[i];
            auto mesh = scene->mMeshes[meshIdx];
            auto vertices = mesh->mVertices;
            auto normals = mesh->mNormals;
            auto tangents = mesh->mTangents;
            auto bitangents = mesh->mBitangents;
            auto uvs = mesh->mTextureCoords[0];
            auto colors = mesh->mColors[0];

            auto mat = scene->mMaterials[mesh->mMaterialIndex];
            addTextures(aiTextureType_DIFFUSE, mat, materialDesc.diffuseTextures);
            addTextures(aiTextureType_HEIGHT, mat, materialDesc.normalTextures);
            addTextures(aiTextureType_SPECULAR, mat, materialDesc.specularTextures);
            addTextures(aiTextureType_AMBIENT, mat, materialDesc.emissionTextures);
            addTextures(aiTextureType_EMISSIVE, mat, materialDesc.emissionTextures);
            addTextures(aiTextureType_OPACITY, mat, materialDesc.opacityTextures);

            setColor(AI_MATKEY_COLOR_DIFFUSE, mat, glm::vec3(1.f), materialDesc.diffuseColor);
            setColor(AI_MATKEY_COLOR_SPECULAR, mat, glm::vec3(1.f), materialDesc.specularColor);
            setColor(AI_MATKEY_COLOR_EMISSIVE, mat, glm::vec3(0.f), materialDesc.emissiveColor);
            setFloat(AI_MATKEY_SHININESS, mat, 0.f, materialDesc.shininess);
            setFloat(AI_MATKEY_OPACITY, mat, 1.0f, materialDesc.opacity);

            for (unsigned int j = 0; j < mesh->mNumVertices; ++j)
            {
                subMesh.vertices.push_back(glm::vec3(vertices[j].x, vertices[j].y, vertices[j].z));

                if (mesh->HasNormals())
                {
                    subMesh.normals.push_back(glm::vec3(normals[j].x, normals[j].y, normals[j].z));
                }

                if (mesh->HasTangentsAndBitangents())
                {
                    subMesh.tangents.push_back(glm::vec3(tangents[j].x, tangents[j].y, tangents[j].z));
					subMesh.bitangents.push_back(glm::vec3(bitangents[j].x, bitangents[j].y, bitangents[j].z));
                }

                if (mesh->HasTextureCoords(0))
                {
                    subMesh.uvs.push_back(glm::vec2(uvs[j].x, uvs[j].y));
                }

                if (mesh->HasVertexColors(0))
                {
                    subMesh.colors.push_back(glm::vec3(colors[j].r, colors[j].g, colors[j].b));
                }
            }

            for (unsigned int k = 0; k < mesh->mNumFaces; ++k)
            {
                auto face = mesh->mFaces[k];

                for (unsigned int j = 0; j < face.mNumIndices; ++j)
                {
                    subMesh.indices.push_back(face.mIndices[j]);
                }
            }
        }
    }

    const aiMatrix4x4& transform = node->mTransformation;
    aiVector3t<float> scale;
    aiVector3t<float> pos;
    aiQuaterniont<float> rotation;
    transform.Decompose(scale, rotation, pos);

	double scaleFactor = 1.0f;

	if (scene->mMetaData)
		scene->mMetaData->Get("UnitScaleFactor", scaleFactor);

    model->position = glm::vec3(pos.x, pos.y, pos.z);
    model->scale = glm::vec3(scale.x, scale.y, scale.z);
    model->rotation = glm::quat(rotation.w, rotation.x, rotation.y, rotation.z);

	// Fill sub meshes to make them compatible with all shaders
	for (auto& subMesh : model->subMeshes)
	{
		subMesh.tangents.resize(subMesh.vertices.size());
		subMesh.bitangents.resize(subMesh.vertices.size());
		subMesh.uvs.resize(subMesh.vertices.size());
	}

    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        auto childModel = process(scene, node->mChildren[i]);
        model->addChild(childModel);
    }

    return model;
}

bool ModelImportProgressHandler::Update(float percentage /*= -1.f*/)
{
	ResourceManager::progress = percentage;
	return true;
}
