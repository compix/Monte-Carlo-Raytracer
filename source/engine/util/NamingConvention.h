#pragma once
#include <cstdint>
#include <string>

class NC
{
public:
    static std::string albedoTexture(uint32_t idx) { return "u_albedoTexture" + std::to_string(idx); }
    static std::string metallicTexture(uint32_t idx) { return "u_metallicTexture" + std::to_string(idx); }
    static std::string diffuseTexture(uint32_t idx) { return "u_diffuseTexture" + std::to_string(idx); }
    static std::string normalMap(uint32_t idx) { return "u_normalMap" + std::to_string(idx); }
    static std::string specularMap(uint32_t idx) { return "u_specularMap" + std::to_string(idx); }
    static std::string emissionMap(uint32_t idx) { return "u_emissionMap" + std::to_string(idx); }
    static std::string opacityMap(uint32_t idx) { return "u_opacityMap" + std::to_string(idx); }

    //static std::string albedoTexture() { return "u_albedoTexture"; }
    //static std::string metallicTexture() { return "u_metallicTexture"; }
    //static std::string diffuseTexture() { return "u_diffuseTexture"; }
    //static std::string normalMap() { return "u_normalMap"; }
    //static std::string specularMap() { return "u_specularMap"; }
    //static std::string emissionMap() { return "u_emissionMap"; }
    //static std::string opacityMap() { return "u_opacityMap"; }

    static std::string color() { return "u_color"; }
    static std::string color(uint32_t idx) { return "u_color" + std::to_string(idx); }
    static std::string emissionColor() { return "u_emissionColor"; }
    static std::string specularColor() { return "u_specularColor"; }
    static std::string shininess() { return "u_shininess"; }

    static std::string hasDiffuseTexture() { return "u_hasDiffuseTexture"; }
    static std::string hasNormalMap() { return "u_hasNormalMap"; }
    static std::string hasSpecularMap() { return "u_hasSpecularMap"; }
    static std::string hasEmissionMap() { return "u_hasEmissionMap"; }
    static std::string hasOpacityMap() { return "u_hasOpacityMap"; }
};
