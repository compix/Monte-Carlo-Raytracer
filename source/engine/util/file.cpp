#include "file.h"
#include <fstream>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <iostream>
#include <GL/glew.h>
#include <engine/resource/ResourceManager.h>
#include <nfd.h>
#include "regex"

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
#include <Windows.h>
#endif

namespace file
{
    Path::Path(const std::string& path)
	{
		m_path = std::regex_replace(std::string(path), std::regex("(\\\\)"), "/");
	}

    std::string Path::getExtension() const
    {
        size_t dotPos = m_path.find_last_of('.');
        if (dotPos < m_path.npos)
            return m_path.substr(dotPos);

        return "";
    }

    std::string Path::getFilename() const
    {
        size_t slashPos = m_path.find_last_of('/') + 1;
        if (slashPos < m_path.npos)
            return m_path.substr(slashPos, m_path.find_last_of('.') - slashPos);

        return m_path;
    }

    std::string Path::getFilenameWithExtension() const
    {
        size_t slashPos = m_path.find_last_of('/') + 1;

        if (slashPos < m_path.npos)
            return m_path.substr(m_path.find_last_of('/') + 1);

        return m_path;
    }

	Path Path::getDirectory() const
	{
		size_t slashPos = m_path.find_last_of('/');

		if (slashPos < m_path.npos)
		{
			return Path(m_path.substr(0, slashPos + 1));
		}

		return Path("");
	}

	Path Path::getParent() const
    {
        size_t slashPos = m_path.find_last_of('/');

        if (slashPos < m_path.npos)
        {
            auto path = m_path.substr(0, slashPos);
            return Path(path.substr(0, path.find_last_of('/') + 1));
        }

        return Path("");
    }
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)

void file::forEachFileInDirectory(const std::string& directoryPath, bool recursive, const file::DirectoryIterationFunction& fileFunc)
{
    WIN32_FIND_DATAA findFileData;
    HANDLE dirHandle = FindFirstFileA((directoryPath + "/*").c_str(), &findFileData);

    if (dirHandle == INVALID_HANDLE_VALUE)
        return;

    do
    {
        std::string filename = findFileData.cFileName;
        std::string fullFilename = directoryPath + "/" + filename;
        bool isDirectory = (findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (filename[0] == '.')
            continue;

        if (isDirectory)
        {
            fileFunc(directoryPath, filename, isDirectory);

            if (recursive)
                forEachFileInDirectory(fullFilename, recursive, fileFunc);

            continue;
        }

        fileFunc(directoryPath, filename, isDirectory);
    } while (FindNextFileA(dirHandle, &findFileData));

    FindClose(dirHandle);
}
#else
// TODO: Write unix version
#endif

file::ShaderSourceInfo resolveShaderIncludes(const std::string& shaderPath, const std::string& source, size_t lineStart)
{
    file::ShaderSourceInfo info(shaderPath, "");
    info.lineStart = lineStart;

    std::istringstream stream(source);

    size_t lineNumber = lineStart;

    std::string line;
    while (std::getline(stream, line))
    {
        size_t incPos = line.find("#include");

        if (incPos != line.npos)
        {
            size_t p0 = line.find_first_of("\"", incPos);
            size_t p1 = line.npos;
            if (p0 != line.npos)
                p1 = line.find_first_of("\"", p0 + 1);

            if (p1 != line.npos && p1 > p0)
            {
                std::string includePath = line.substr(p0 + 1, p1 - p0 - 1);
                auto& includeSource = ResourceManager::getIncludeSource(includePath);
                file::ShaderSourceInfo includeInfo = resolveShaderIncludes(includePath, includeSource, lineNumber);
                info.children.push_back(includeInfo);
                info.source += includeInfo.source + "\n";

                lineNumber += includeInfo.lineEnd - includeInfo.lineStart;
            }
            else
            {
                std::cout << "Failed to parse " << shaderPath << ": unexpected #include input at line: " << lineNumber << std::endl;
            }
        }
		else if (line.find("GL_ARB_shading_language_include") != line.npos)
		{
			info.source += "\n";
		}
		else
		{
			info.source += line + "\n";
		}

        ++lineNumber;
    }

    info.lineEnd = lineNumber;

    return info;
}

file::ShaderSourceInfo file::getShaderSource(const std::string& path)
{
    std::string source = readAsString(path);
    if (glewIsSupported("GL_ARB_shading_language_include") == GL_TRUE)
        return ShaderSourceInfo(path, source);

    return resolveShaderIncludes(path, source, 1);
}

std::string file::readAsString(const std::string& path)
{
    std::string fileAsString = "";
    std::ifstream stream(path, std::ios::in);

    if (!stream.is_open())
    {
        std::cerr << "Could not open file: " << path << std::endl;
        return "";
    }

    std::string line = "";
    while (getline(stream, line))
        fileAsString += line + "\n";

    stream.close();

    return fileAsString;
}

void file::saveToFile(const std::string& path, const std::string& text)
{
    std::string fileAsString = "";
    std::ofstream stream(path, std::ios::out);

    if (!stream.is_open())
    {
        std::cerr << "Could not open file: " << path << std::endl;
        return;
    }

    stream << text;
    stream.close();
}

void file::loadRawBuffer(const std::string& path, std::vector<char>& outBuffer, uint32_t& outNumValues)
{
    std::ifstream input(path, std::ios::binary);
    outBuffer = { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    outNumValues = *reinterpret_cast<uint32_t*>(&outBuffer[0]);
}

bool file::exists(const std::string& filename) noexcept
{
    struct stat buffer;
    return stat(filename.c_str(), &buffer) == 0;
}

size_t file::getSize(const std::string& filename)
{
    struct stat buffer;
    return stat(filename.c_str(), &buffer) == 0 ? buffer.st_size : 0;
}

bool file::openFileDialog(const std::string& filterList, const std::string& defaultPath, std::string& outPath)
{
	nfdchar_t * outPathCStr = nullptr;
	nfdresult_t result = NFD_OpenDialog(filterList == "" ? nullptr : filterList.c_str(), 
		                                defaultPath == "" ? nullptr : defaultPath.c_str(), &outPathCStr);

	if (result == NFD_OKAY) {
		outPath = outPathCStr;
		outPath = std::regex_replace(std::string(outPath), std::regex("(\\\\)"), "/");
		free(outPathCStr);
		return true;
	}
	else if (result == NFD_CANCEL) {
	}
	else {
		LOG_ERROR("Native File Dialog Error: " << NFD_GetError());
	}

	return false;
}

bool file::openDirectoryPickerDialog(const std::string& defaultPath, std::string& outPath)
{
	nfdchar_t * outPathCStr = nullptr;
	nfdresult_t result = NFD_PickFolder(defaultPath == "" ? nullptr : defaultPath.c_str(), &outPathCStr);

	if (result == NFD_OKAY) {
		outPath = outPathCStr;
		outPath = std::regex_replace(std::string(outPath), std::regex("(\\\\)"), "/");
		outPath += "/";

		free(outPathCStr);
		return true;
	}
	else if (result == NFD_CANCEL) {
	}
	else {
		LOG_ERROR("Native File Dialog Error: " << NFD_GetError());
	}

	return false;
}

bool file::openDirectoryPickerDialog(std::string& outPath)
{
	return openDirectoryPickerDialog("", outPath);
}

std::string file::relativeToAbsolutePath(const std::string& relativePath)
{
	char absPath[_MAX_PATH];
	_fullpath(absPath, relativePath.c_str(), _MAX_PATH);
	return std::regex_replace(std::string(absPath), std::regex("(\\\\)"), "/");
}

void file::createDirectory(const std::string& directoryPath)
{
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32)
	CreateDirectory(directoryPath.c_str(), NULL);
#endif
}

bool file::openFileDialog(std::string& outPath)
{
	return openFileDialog("", "", outPath);
}
