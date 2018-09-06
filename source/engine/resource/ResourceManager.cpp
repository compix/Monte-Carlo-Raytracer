#include "ResourceManager.h"
#include "AssetImporter.h"
#include <engine/rendering/Material.h>
#include <engine/rendering/renderer/MeshRenderer.h>
#include "engine/util/file.h"
#include "serialization.h"
#include "../util/Timer.h"
#include "../util/Logger.h"
#include "future"

#define ASSET_CACHE_FOLDER std::string(ASSET_ROOT_FOLDER) + "cache/"
#define ASSET_MODEL_CACHE_FOLDER ASSET_CACHE_FOLDER + "models/"
#define ASSET_CACHE_EXTENSION ".cache"

std::unordered_map<Texture2DKey, std::shared_ptr<Texture2D>> ResourceManager::m_textures2D;
std::unordered_map<TextureID, std::shared_ptr<Texture2D>> ResourceManager::m_glIdToTexture2D;
std::unordered_map<std::string, std::shared_ptr<Model>> ResourceManager::m_models;
std::unordered_map<ShaderKey, std::shared_ptr<Shader>> ResourceManager::m_shaders;

std::unordered_map<Model*, std::shared_ptr<Mesh>> ResourceManager::m_meshes;

std::unordered_map<std::string, std::string> ResourceManager::m_shaderIncludes;

std::mutex ResourceManager::m_modelImportMutex;

std::atomic_int ResourceManager::m_loadingTaskCounter = 0;

ProgressBar ResourceManager::progress = ProgressBar("Model Import Progress");

void ResourceManager::addLoadingTasks(int increment)
{
	m_loadingTaskCounter += increment;
}

bool ResourceManager::isLoading()
{
	return m_loadingTaskCounter > 0;
}

void ResourceManager::init()
{
	// Create cache folders
	file::createDirectory(ASSET_CACHE_FOLDER);
	file::createDirectory(ASSET_MODEL_CACHE_FOLDER);
}

std::shared_ptr<Texture2D> ResourceManager::getTexture(const std::string& path, bool isRelativeAssetPath, Texture2DSettings settings)
{
	const std::string fullPath = isRelativeAssetPath ? (std::string(ASSET_ROOT_FOLDER) + path) : path;

    auto key = Texture2DKey(fullPath, settings);
    auto it = m_textures2D.find(key);
    if (it != m_textures2D.end())
        return it->second;

    auto texture = std::make_shared<Texture2D>(fullPath, settings);
    m_textures2D[key] = texture;
	m_glIdToTexture2D[texture->getGLID()] = texture;

    return texture;
}

std::shared_ptr<Texture2D> ResourceManager::getTextureFromGLId(TextureID id)
{
	auto itr = m_glIdToTexture2D.find(id);
	if (itr != m_glIdToTexture2D.end())
		return itr->second;

	return nullptr;
}

std::shared_ptr<AsyncFuture<std::shared_ptr<Model>>> ResourceManager::getModelAsync(const std::string& path, const ModelImportSettings& importSettings)
{
	std::lock_guard<std::mutex> lock(m_modelImportMutex);

	auto modelImportFuture = std::make_shared<AsyncFuture<std::shared_ptr<Model>>>();

	const std::string fullPath = importSettings.isRelativeAssetPath ? (std::string(ASSET_ROOT_FOLDER) + path) : path;
	if (!file::exists(fullPath))
	{
		LOG_ERROR("File " << fullPath << " does not exist.");
		modelImportFuture->setState(ProgressState::Failed);
		return modelImportFuture;
	}

	auto it = m_models.find(fullPath);
	if (it != m_models.end())
	{
		modelImportFuture->onCompleted(it->second);
		return modelImportFuture;
	}

	static const std::hash<std::string> hashFn;
	size_t hashVal = hashFn(fullPath);
	std::string cachedPath = std::string(ASSET_MODEL_CACHE_FOLDER) + std::to_string(hashVal) + ASSET_CACHE_EXTENSION;

	if (file::exists(cachedPath))
	{
		static std::future<void> cachedFuture;
		cachedFuture = std::async(std::launch::async, [cachedPath, fullPath, modelImportFuture]() {
			addLoadingTasks(1);
			auto cachedModel = Serializer::readModel(cachedPath);
			modelImportFuture->onCompleted(cachedModel);
			m_models[fullPath] = cachedModel;
			progress = 1.0f;
			addLoadingTasks(-1);
		});
	}
	else
	{
		modelImportFuture = AssetImporter::importAsync(fullPath, importSettings);
		modelImportFuture->addOnCompletionListener([cachedPath, fullPath](std::shared_ptr<Model>& m) {
			Serializer::write(cachedPath, *m);
			m_models[fullPath] = m;
		});
	}

	return modelImportFuture;
}

std::shared_ptr<Model> ResourceManager::getModel(const std::string& path, const ModelImportSettings& importSettings)
{
	const std::string fullPath = importSettings.isRelativeAssetPath ? (std::string(ASSET_ROOT_FOLDER) + path) : path;
	if (!file::exists(fullPath))
	{
		LOG_ERROR("File " << fullPath << " does not exist.");
		return std::shared_ptr<Model>();
	}

    auto it = m_models.find(fullPath);
    if (it != m_models.end())
        return it->second;

	std::shared_ptr<Model> model;

	static const std::hash<std::string> hashFn;
	size_t hashVal = hashFn(fullPath);
	std::string cachedPath = std::string(ASSET_MODEL_CACHE_FOLDER) + std::to_string(hashVal) + ASSET_CACHE_EXTENSION;

	LOG("Loading " << path << " ...");

	Timer timer;
	timer.start();
	if (file::exists(cachedPath))
	{
		model = Serializer::readModel(cachedPath);
	}
	else
	{
		model = AssetImporter::import(fullPath, importSettings);
		Serializer::write(cachedPath, *model);
	}

	timer.tick();
	auto timeInSeconds = timer.totalTimeInMicroseconds() / 1000 / 1000;
	LOG("Done in " << timeInSeconds << " seconds.");

	m_models[fullPath] = model;
    return model;
}

std::shared_ptr<Mesh> ResourceManager::getMesh(Model* model)
{
	assert(model);

	auto it = m_meshes.find(model);

	if (it != m_meshes.end())
		return it->second;

	auto mesh = model->createMesh();
	m_meshes[model] = mesh;
	return mesh;
}

std::shared_ptr<Shader> ResourceManager::getShader(const std::string& vsPath, const std::string& fsPath, std::initializer_list<std::string> vertexAttributeNames)
{
    auto key = ShaderKey(vsPath, fsPath);
    auto it = m_shaders.find(key);
    if (it != m_shaders.end())
        return it->second;

    std::shared_ptr<Shader> shader = std::make_shared<Shader>();

    shader->load(ASSET_ROOT_FOLDER + vsPath, ASSET_ROOT_FOLDER + fsPath, vertexAttributeNames);
    m_shaders[key] = shader;

    return shader;
}

std::shared_ptr<Shader> ResourceManager::getShader(const std::string& vsPath, const std::string& fsPath, const std::string& gsPath)
{
    auto key = ShaderKey(vsPath, fsPath, gsPath);
    auto it = m_shaders.find(key);
    if (it != m_shaders.end())
        return it->second;

    std::shared_ptr<Shader> shader = std::make_shared<Shader>();

    shader->load(ASSET_ROOT_FOLDER + vsPath, ASSET_ROOT_FOLDER + fsPath, ASSET_ROOT_FOLDER + gsPath);
    m_shaders[key] = shader;

    return shader;
}

std::shared_ptr<Shader> ResourceManager::getComputeShader(const std::string& path)
{
    auto key = ShaderKey(path);
    auto it = m_shaders.find(key);
    if (it != m_shaders.end())
        return it->second;

    std::shared_ptr<Shader> shader = std::make_shared<Shader>();

    shader->loadCompute(ASSET_ROOT_FOLDER + path);
    m_shaders[key] = shader;

    return shader;
}

void ResourceManager::setShaderIncludePath(const std::string& path)
{
    const std::string fullPath = ASSET_ROOT_FOLDER + path;

    if (!file::exists(fullPath))
    {
        LOG_ERROR("The path " << path << " does not exist.");
        return;
    }

    size_t pathLength = fullPath.length();

    file::forEachFileInDirectory(fullPath, true, [pathLength](const std::string& directoryPath, const std::string& filename, bool isDirectory) 
    {
        if (!isDirectory)
        {
            file::Path p(filename);

            if (p.getExtension() == ".glsl")
            {
                std::string fPath = directoryPath + "/" + filename;
                std::string rPath(fPath.substr(pathLength));
                std::string str = file::readAsString(fPath);

                if (glewIsSupported("GL_ARB_shading_language_include") == GL_TRUE)
                    glNamedStringARB(GL_SHADER_INCLUDE_ARB, static_cast<GLint>(rPath.length()), rPath.c_str(), static_cast<GLint>(str.length()), str.c_str());
                else
                    m_shaderIncludes[rPath] = str;
            }
        }
    });
}

const std::string& ResourceManager::getIncludeSource(const std::string& includePath)
{
    return m_shaderIncludes[includePath];
}
