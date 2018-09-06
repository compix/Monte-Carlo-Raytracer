#pragma once
#include <fstream>
#include <memory>
#include <engine/rendering/geometry/Mesh.h>
#include "Model.h"

struct Model;
class Transform;

class Serializer
{
public:
    // Generic
    template <class T>
    static void write(std::ofstream& os, const T& val) { os.write(reinterpret_cast<const char*>(&val), sizeof(T)); }

	template <>
	static void write<std::string>(std::ofstream& os, const std::string& s)
	{
		write(os, s.size());
		os.write(s.c_str(), sizeof(char) * s.size());
	}

	template <>
	static void write<MaterialDescription>(std::ofstream& os, const MaterialDescription& md);

    template <class T>
    static void writeVector(std::ofstream& os, const std::vector<T>& v);

    template <class T>
    static void read(std::ifstream& is, T& val) { is.read(reinterpret_cast<char*>(&val), sizeof(T)); }

	template<>
	static void read<MaterialDescription>(std::ifstream& is, MaterialDescription& md);

    template <class T>
    static T read(std::ifstream& is)
    {
        T val;
		read(is, val);
        return val;
    }

    template <class T>
    static void readVector(std::ifstream& is, std::vector<T>& v);

	template<>
	static void read<std::string>(std::ifstream& is, std::string& s);

    // Model
    static void write(std::ofstream& os, const Model& val);
    static std::shared_ptr<Model> readModel(std::ifstream& is);

	static std::shared_ptr<Model> readModel(const std::string& absPath);
	static void write(const std::string& absPath, const Model& model);
};

template <class T>
void Serializer::writeVector(std::ofstream& os, const std::vector<T>& v)
{
    write(os, v.size());
    for (auto& val : v)
        write(os, val);
}

template <class T>
void Serializer::readVector(std::ifstream& is, std::vector<T>& v)
{
    v.resize(read<size_t>(is));
    for (size_t i = 0; i < v.size(); ++i)
        read<T>(is, v[i]);
}

template<>
void Serializer::read(std::ifstream& is, std::string& s)
{
	size_t size = read<size_t>(is);
	char* chars = new char[size + 1];
	is.read(chars, size);
	chars[size] = '\0';
	s = chars;

	delete[] chars;
}

template<>
void Serializer::read(std::ifstream& is, MaterialDescription& md)
{
	readVector(is, md.diffuseTextures);
	readVector(is, md.normalTextures);
	readVector(is, md.specularTextures);
	readVector(is, md.emissionTextures);
	readVector(is, md.opacityTextures);

	read(is, md.diffuseColor);
	read(is, md.specularColor);
	read(is, md.emissiveColor);
	read(is, md.opacity);
	read(is, md.shininess);
}

template <>
void Serializer::write(std::ofstream& os, const MaterialDescription& md)
{
	writeVector(os, md.diffuseTextures);
	writeVector(os, md.normalTextures);
	writeVector(os, md.specularTextures);
	writeVector(os, md.emissionTextures);
	writeVector(os, md.opacityTextures);

	write(os, md.diffuseColor);
	write(os, md.specularColor);
	write(os, md.emissiveColor);
	write(os, md.opacity);
	write(os, md.shininess);
}
