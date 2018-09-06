#include "serialization.h"
#include "Model.h"

// ****************************** Model ******************************
void Serializer::write(std::ofstream& os, const Model& model)
{
	write(os, model.name);
    write(os, model.position);
    write(os, model.scale);
    write(os, model.rotation);

    // Number of SubMeshes
    write(os, model.subMeshes.size());

    // SubMehes
    for (auto& sm : model.subMeshes)
    {
        writeVector(os, sm.indices);
        writeVector(os, sm.vertices);
        writeVector(os, sm.normals);
        writeVector(os, sm.tangents);
		writeVector(os, sm.bitangents);
        writeVector(os, sm.uvs);
        writeVector(os, sm.colors);
    }

	writeVector(os, model.materials);

    // Number of children
    write(os, model.children.size());

    // Children
    for (auto& child : model.children) { write(os, *child); }
}

void Serializer::write(const std::string& absPath, const Model& model)
{
	std::ofstream os(absPath, std::ios::binary);
	if (os.is_open())
	{
		write(os, model);
	}
	else
	{
		LOG_ERROR("Failed to create file " << absPath);
	}
}

std::shared_ptr<Model> Serializer::readModel(std::ifstream& is)
{
    std::shared_ptr<Model> model = std::make_shared<Model>();

	read(is, model->name);
    model->position = read<glm::vec3>(is);
    model->scale = read<glm::vec3>(is);
    model->rotation = read<glm::quat>(is);

    // Number of SubMeshes
    model->subMeshes.resize(read<size_t>(is));

    // SubMeshes
    for (size_t i = 0; i < model->subMeshes.size(); ++i)
    {
        Mesh::SubMesh& subMesh = model->subMeshes[i];

        readVector(is, subMesh.indices);
        readVector(is, subMesh.vertices);
        readVector(is, subMesh.normals);
        readVector(is, subMesh.tangents);
		readVector(is, subMesh.bitangents);
        readVector(is, subMesh.uvs);
        readVector(is, subMesh.colors);
    }

	readVector(is, model->materials);

    // Number of children
    model->children.resize(read<size_t>(is));

    // Children
    for (size_t i = 0; i < model->children.size(); ++i)
    {
        model->children[i] = readModel(is);
        model->children[i]->parent = model.get();
    }

    return model;
}

std::shared_ptr<Model> Serializer::readModel(const std::string& absPath)
{
	std::ifstream is(absPath, std::ios::binary);
	return readModel(is);
}