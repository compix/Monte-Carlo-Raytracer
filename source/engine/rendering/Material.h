#pragma once
#include <string>
#include <unordered_map>
#include <glm/detail/type_vec3.hpp>
#include <memory>
#include "shader/Shader.h"
#include "Texture2D.h"
#include "engine/resource/Model.h"

using TextureName = std::string;

struct EditableMaterialDesc
{
    EditableMaterialDesc() {}
    EditableMaterialDesc(float min, float max)
        :min(min), max(max) {}
    EditableMaterialDesc(bool isColor)
        :isColor(isColor) {}

    float min{0.0f};
    float max{0.0f};
    bool isColor{ false };
};

class EditableMaterialProperties
{
public:
    static void init();

    static const EditableMaterialDesc* getDesc(const UniformName& uniformName)
    {
        auto it = m_materialDescs.find(uniformName);
        if (it != m_materialDescs.end())
            return &it->second;

        return nullptr;
    }

private:

    static std::unordered_map<UniformName, EditableMaterialDesc> m_materialDescs;
};

class Material
{
    friend class MeshRenderer;
    friend class MeshRenderSystem;
public:
    Material() { }

    explicit Material(std::shared_ptr<Shader> shader);

    void setShader(std::shared_ptr<Shader> shader);

    void setTexture2D(const TextureName& textureName, TextureID textureID);
    void setTexture3D(const TextureName& textureName, TextureID textureID);

    TextureID getTexture2D(const TextureName& textureName) { return m_textures2D[textureName]; }
    TextureID getTexture3D(const TextureName& textureName) { return m_textures3D[textureName]; }

    bool tryGetTexture2D(const TextureName& textureName, TextureID& outTex) const { return tryGet(textureName, m_textures2D, outTex); }
    bool tryGetTexture3D(const TextureName& textureName, TextureID& outTex) const { return tryGet(textureName, m_textures3D, outTex); }

    std::shared_ptr<Shader> getShader() const { return m_shader; }

    void setFloat(const UniformName& uniformName, float v) noexcept { m_floatMap[uniformName] = v; }

    void setVector(const UniformName& uniformName, const glm::vec2& v) noexcept { m_vec2Map[uniformName] = v; }

    void setVector(const UniformName& uniformName, const glm::vec3& v) noexcept { m_vec3Map[uniformName] = v; }

    void setVector(const UniformName& uniformName, const glm::vec4& v) noexcept { m_vec4Map[uniformName] = v; }

    void setColor(const UniformName& uniformName, const glm::vec3& v) noexcept { m_vec3Map[uniformName] = v; }

    void setColor(const UniformName& uniformName, const glm::vec4& v) noexcept { m_vec4Map[uniformName] = v; }

    void setMatrix(const UniformName& uniformName, const glm::mat2& m) noexcept { m_mat2Map[uniformName] = m; }

    void setMatrix(const UniformName& uniformName, const glm::mat3& m) noexcept { m_mat3Map[uniformName] = m; }

    void setMatrix(const UniformName& uniformName, const glm::mat4& m) noexcept { m_mat4Map[uniformName] = m; }

    // Getters
    float getFloat(const UniformName& uniformName) const noexcept { return m_floatMap.at(uniformName); }
    const glm::vec2& getVector2(const UniformName& uniformName) const noexcept { return m_vec2Map.at(uniformName); }
    const glm::vec3& getVector3(const UniformName& uniformName) const noexcept { return m_vec3Map.at(uniformName); }
    const glm::vec4& getVector4(const UniformName& uniformName) const noexcept { return m_vec4Map.at(uniformName); }
    const glm::vec3& getColor3(const UniformName& uniformName) const noexcept { return m_vec3Map.at(uniformName); }
    const glm::vec4& getColor(const UniformName& uniformName) const noexcept { return m_vec4Map.at(uniformName); }
    const glm::mat2& getMatrix2(const UniformName& uniformName) const noexcept { return m_mat2Map.at(uniformName); }
    const glm::mat3& getMatrix3(const UniformName& uniformName) const noexcept { return m_mat3Map.at(uniformName); }
    const glm::mat4& getMatrix4(const UniformName& uniformName) const noexcept { return m_mat4Map.at(uniformName); }

    bool tryGetFloat(const UniformName& uniformName, float& outVal) const noexcept { return tryGet(uniformName, m_floatMap, outVal); }
    bool tryGetVector2(const UniformName& uniformName, glm::vec2& outVal) const noexcept { return tryGet(uniformName, m_vec2Map, outVal); }
    bool tryGetVector3(const UniformName& uniformName, glm::vec3& outVal) const noexcept { return tryGet(uniformName, m_vec3Map, outVal); }
    bool tryGetVector4(const UniformName& uniformName, glm::vec4& outVal) const noexcept { return tryGet(uniformName, m_vec4Map, outVal); }
    bool tryGetColor3(const UniformName& uniformName, glm::vec3& outVal) const noexcept { return tryGet(uniformName, m_vec3Map, outVal); }
    bool tryGetColor(const UniformName& uniformName, glm::vec4& outVal) const noexcept { return tryGet(uniformName, m_vec4Map, outVal); }
    bool tryGetMatrix2(const UniformName& uniformName, glm::mat2& outVal) const noexcept { return tryGet(uniformName, m_mat2Map, outVal); }
    bool tryGetMatrix3(const UniformName& uniformName, glm::mat3& outVal) const noexcept { return tryGet(uniformName, m_mat3Map, outVal); }
    bool tryGetMatrix4(const UniformName& uniformName, glm::mat4& outVal) const noexcept { return tryGet(uniformName, m_mat4Map, outVal); }

    const MaterialDescription& getDescription() const { return m_desc; }
private:
    void use();
    void use(Shader* shader, bool bind = false);

    template<class T, class MapT>
    bool tryGet(const std::string& name, MapT& map, T& outVal) const
    {
        auto it = map.find(name);
        if (it != map.end())
        {
            outVal = it->second;
            return true;
        }

        return false;
    }
private:
    MaterialDescription m_desc;
    std::shared_ptr<Shader> m_shader;
    std::unordered_map<TextureName, TextureID> m_textures2D;
    std::unordered_map<TextureName, TextureID> m_textures3D;

    std::unordered_map<UniformName, float> m_floatMap;
    std::unordered_map<UniformName, glm::vec2> m_vec2Map;
    std::unordered_map<UniformName, glm::vec3> m_vec3Map;
    std::unordered_map<UniformName, glm::vec4> m_vec4Map;
    std::unordered_map<UniformName, glm::mat2> m_mat2Map;
    std::unordered_map<UniformName, glm::mat3> m_mat3Map;
    std::unordered_map<UniformName, glm::mat4> m_mat4Map;
};
