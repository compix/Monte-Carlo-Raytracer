#pragma once
#include <engine/rendering/Texture2D.h>
#include <memory>
#include <unordered_map>
#include "Model.h"
#include <engine/rendering/shader/Shader.h>
#include "AssetImporter.h"
#include "mutex"
#include "atomic"

class Material;

struct Texture2DKey
{
    Texture2DKey() { }

    Texture2DKey(const std::string& texturePath, Texture2DSettings settings)
        : path(texturePath), settings(settings) { }

    bool operator ==(const Texture2DKey& otherKey) const { return otherKey.path == path && settings == otherKey.settings; }

    std::string path;
    Texture2DSettings settings{ Texture2DSettings::S_T_REPEAT_MIN_MAG_LINEAR };
};

struct ShaderKey
{
    ShaderKey() { }

    ShaderKey(const std::string& vsPath, const std::string& fsPath)
        : vsPath(vsPath), fsCompPath(fsPath) { }

    ShaderKey(const std::string& vsPath, const std::string& fsPath, const std::string& gsPath)
        : vsPath(vsPath), fsCompPath(fsPath), gsPath(gsPath) { }

    ShaderKey(const std::string& computePath)
        : fsCompPath(computePath) { }

    bool operator ==(const ShaderKey& otherKey) const { return otherKey.vsPath == vsPath && otherKey.fsCompPath == fsCompPath && otherKey.gsPath == gsPath; }

    std::string vsPath;
    // Either fs or compute shader path
    std::string fsCompPath;
    std::string gsPath;
};

namespace std
{
    template <>
    struct hash<Texture2DKey>
    {
        std::size_t operator()(const Texture2DKey& key) const { return std::hash<std::string>()(key.path); }
    };

    template <>
    struct hash<ShaderKey>
    {
        std::size_t operator()(const ShaderKey& key) const
        {
            return ((hash<string>()(key.vsPath)
                ^ (hash<string>()(key.fsCompPath) << 1)) >> 1)
                ^ (hash<string>()(key.gsPath) << 1);
        }
    };
}

class ResourceManager
{
public:
	static void init();

	static std::shared_ptr<Texture2D> getTexture(const std::string& path, bool isRelativeAssetPath = true,
		Texture2DSettings settings = Texture2DSettings::S_T_REPEAT_MIN_MAG_LINEAR);

	static std::shared_ptr<Texture2D> getTextureFromGLId(TextureID id);

	static std::shared_ptr<AsyncFuture<std::shared_ptr<Model>>> getModelAsync(const std::string& path, const ModelImportSettings& importSettings);
    static std::shared_ptr<Model> getModel(const std::string& path, const ModelImportSettings& importSettings);
	static std::shared_ptr<Mesh> getMesh(Model* model);

    static std::shared_ptr<Shader> getShader(const std::string& vsPath, const std::string& fsPath, std::initializer_list<std::string> vertexAttributeNames = {});
    static std::shared_ptr<Shader> getShader(const std::string& vsPath, const std::string& fsPath, const std::string& gsPath);
    static std::shared_ptr<Shader> getComputeShader(const std::string& path);

    static void setShaderIncludePath(const std::string& path);
    static const std::string& getIncludeSource(const std::string& includePath);

    static std::vector<std::shared_ptr<Texture2D>> getTextures2D()
    {
        std::vector<std::shared_ptr<Texture2D>> textures;
        for (auto& p : m_textures2D)
            textures.push_back(p.second);

        return textures;
    }

	static ProgressBar progress;

	static void addLoadingTasks(int increment);
	static bool isLoading();
private:
    static std::unordered_map<Texture2DKey, std::shared_ptr<Texture2D>> m_textures2D;
	static std::unordered_map<TextureID, std::shared_ptr<Texture2D>> m_glIdToTexture2D;
    static std::unordered_map<std::string, std::shared_ptr<Model>> m_models;
    static std::unordered_map<ShaderKey, std::shared_ptr<Shader>> m_shaders;
	static std::unordered_map<Model*, std::shared_ptr<Mesh>> m_meshes;

    static std::unordered_map<std::string, std::string> m_shaderIncludes;

	static std::mutex m_modelImportMutex;
	static std::atomic_int m_loadingTaskCounter;
};
