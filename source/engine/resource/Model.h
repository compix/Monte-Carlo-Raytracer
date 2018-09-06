#pragma once
#include <vector>
#include <engine/geometry/Transform.h>
#include <engine/rendering/geometry/Mesh.h>
#include <memory>

struct MaterialDescription
{
    std::vector<std::string> diffuseTextures;
    std::vector<std::string> normalTextures;
    std::vector<std::string> specularTextures;
    std::vector<std::string> emissionTextures;
    std::vector<std::string> opacityTextures;

    glm::vec3 diffuseColor{ 1.0f };
    glm::vec3 specularColor{ 1.0f };
    glm::vec3 emissiveColor;
    float opacity;
    float shininess;
};

struct Model
{
    void addChild(std::shared_ptr<Model> model);
	glm::vec3 computeCenter() const;
	void translateSubMeshes(const glm::vec3& t);
	void scaleSubMeshes(const glm::vec3& s);

	/**
	* May result in wrong offsets of children.
	*/
	void setOriginToModelCenter(bool processChildren = true);
	void mapToUnitCube(bool mapChildren = true);
	BBox computeBBox();
	std::shared_ptr<Mesh> createMesh();

	std::vector<Mesh::SubMesh> getAllSubMeshes() const;
    std::vector<MaterialDescription> getAllMaterials() const;
    size_t getTriangleCount() const;

	size_t getNumberOfChildren(bool recursive = true);

    // Members
    std::string name;
    glm::vec3 position;
    glm::vec3 scale{1.0f,1.0f,1.0f};
    glm::quat rotation;

    // Index of sub mesh corresponds to the index of the material
    std::vector<Mesh::SubMesh> subMeshes;
    std::vector<MaterialDescription> materials;

    std::vector<std::shared_ptr<Model>> children;
    Model* parent;
};
