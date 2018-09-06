#pragma once
#include <string>
#include <vector>
#include <functional>

namespace file
{
    /**
    * Provides information about the source: the path and source code as a string.
    * If GL_ARB_shading_language_include is not supported it also contains information about
    * #includes.
    */
    struct ShaderSourceInfo
    {
        ShaderSourceInfo() {}
        explicit ShaderSourceInfo(const std::string& path, const std::string& source)
            :path(path), source(source) {}

        /**
        * Returns info for the provided lineNumber.
        */
        const ShaderSourceInfo& getInfo(size_t lineNumber) const
        {
            for (auto& c : children)
                if (c.isLinePartOf(lineNumber))
                    return c.getInfo(lineNumber);

            return *this;
        }

        bool isLinePartOf(size_t lineNumber) const { return lineNumber >= lineStart && lineNumber <= lineEnd; }

        size_t getRelativeLineNumber(size_t lineNumber) const
        {
            size_t i = 0;
            size_t sum = 0;
            for (; i < children.size(); ++i)
            {
                if (lineNumber < children[i].lineEnd)
                    break;
                
                sum += i > 0 ? (children[i].lineStart - children[i - 1].lineEnd)
                             : (children[i].lineStart - lineStart);
            }

            return i > 0 ? (sum + lineNumber - children[i - 1].lineEnd + 1) : (lineNumber - lineStart + 1);
        }

        std::string path;
        std::string source;

        size_t lineStart{ 0 };
        size_t lineEnd{ 0 };
        std::vector<ShaderSourceInfo> children;
    };

    /**
    * Only '/' is allowed to indicate subfolders/child files.
    * A directory has always a '/' at the end of the path string.
    */
    class Path
    {
    private:
        friend class FileSystem;

    public:
        Path(const std::string& path);

        std::string getExtension() const;
        std::string getFilename() const;
        std::string getFilenameWithExtension() const;
		Path getDirectory() const;

        const std::string& asString() const { return m_path; }

        bool isDirectory() const { return m_path.length() > 0 && m_path[m_path.length() - 1] == '/'; }

        Path getParent() const;

		operator std::string() const { return m_path; }

        friend std::ostream& operator<<(std::ostream& os, const Path& path);

    private:
        std::string m_path;
    };

    class File
    {
    public:
        explicit File(const Path& path)
            :m_path(path) {}

    private:
        Path m_path;
    };

    using DirectoryIterationFunction = std::function<void(const std::string& directoryPath, const std::string& filename, bool isDirectory)>;

    void forEachFileInDirectory(const std::string& directoryPath, bool recursive, const file::DirectoryIterationFunction& fileFunc);

    ShaderSourceInfo getShaderSource(const std::string& path);
    std::string readAsString(const std::string& path);
    void saveToFile(const std::string& path, const std::string& text);

    void loadRawBuffer(const std::string& path, std::vector<char>& outBuffer, uint32_t& outNumValues);

    bool exists(const std::string& filename) noexcept;
    size_t getSize(const std::string& filename);

	bool openFileDialog(const std::string& filterList, const std::string& defaultPath, std::string& outPath);
	bool openFileDialog(std::string& outPath);
	bool openDirectoryPickerDialog(const std::string& defaultPath, std::string& outPath);
	bool openDirectoryPickerDialog(std::string& outPath);

	std::string relativeToAbsolutePath(const std::string& relativePath);

	void createDirectory(const std::string& directoryPath);
}

namespace file
{
    inline std::ostream& operator<<(std::ostream& os, const Path& path)
    {
        return os << path.m_path;
    }

}
