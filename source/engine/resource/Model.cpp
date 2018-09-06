#include "Model.h"

void Model::addChild(std::shared_ptr<Model> model)
{
    children.push_back(model);
    model->parent = this;
}

glm::vec3 Model::computeCenter() const
{
	size_t count = 0;
	glm::vec3 center;
	for (auto& subMesh : subMeshes)
	{
		for (auto& v : subMesh.vertices)
		{
			center += v;
			++count;
		}
	}

	return count > 0 ? center / count : center;
}

void Model::translateSubMeshes(const glm::vec3& t)
{
	for (auto& subMesh : subMeshes)
		for (auto& v : subMesh.vertices)
			v += t;
}

void Model::scaleSubMeshes(const glm::vec3& s)
{
	for (auto& subMesh : subMeshes)
		for (auto& v : subMesh.vertices)
			v *= s;
}

void Model::mapToUnitCube(bool mapChildren)
{
	if (subMeshes.size() > 0)
	{
		BBox modelBBox = computeBBox();
		float scaleInv = 1.0f / modelBBox.scale()[modelBBox.maxExtentIdx()];
		glm::vec3 offset = -modelBBox.min();

		for (auto& subMesh : subMeshes)
		{
			for (size_t i = 0; i < subMesh.vertices.size(); ++i)
			{
				subMesh.vertices[i] = (subMesh.vertices[i] + offset) * scaleInv;
			}
		}
	}

	if (mapChildren)
	{
		for (auto& child : children)
		{
			child->mapToUnitCube(mapChildren);
		}
	}
}

BBox Model::computeBBox()
{
	BBox box;

	for (auto& subMesh : subMeshes)
		for (auto& v : subMesh.vertices)
			box.unite(v);

	return box;
}

std::shared_ptr<Mesh> Model::createMesh()
{
	if (subMeshes.size() == 0)
		return std::shared_ptr<Mesh>();

	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
	mesh->setSubMeshes(subMeshes);
	mesh->finalize();

	return mesh;
}

void Model::setOriginToModelCenter(bool processChildren /*= true*/)
{
	auto center = computeCenter();
	translateSubMeshes(-center);
	position = center;

	if (processChildren)
	{
		for (auto& child : children)
			child->setOriginToModelCenter(processChildren);
	}
}

std::vector<Mesh::SubMesh> Model::getAllSubMeshes() const
{
    std::vector<Mesh::SubMesh> allMeshes;
    allMeshes.insert(allMeshes.end(), subMeshes.begin(), subMeshes.end());

    for (auto m : children)
    {
        auto childMeshes = m->getAllSubMeshes();
        allMeshes.insert(allMeshes.end(), childMeshes.begin(), childMeshes.end());
    }

    return allMeshes;
}

std::vector<MaterialDescription> Model::getAllMaterials() const
{
    std::vector<MaterialDescription> allMaterials;
    allMaterials.insert(allMaterials.end(), materials.begin(), materials.end());

    for (auto m : children)
    {
        auto childMaterials = m->getAllMaterials();
        allMaterials.insert(allMaterials.end(), childMaterials.begin(), childMaterials.end());
    }

    return allMaterials;
}

size_t Model::getTriangleCount() const
{
    size_t triangleCount = 0;

    for (auto& sm : subMeshes)
        triangleCount += sm.indices.size() / 3;

    for (auto& c : children)
        triangleCount += c->getTriangleCount();

    return triangleCount;
}

size_t Model::getNumberOfChildren(bool recursive /*= true*/)
{
	size_t num = children.size();

	if (recursive)
		for (auto& child : children)
			num += child->getNumberOfChildren(true);

	return num;
}
