#include "Texture2D.h"
#include <SOIL2.h>
#include <engine/util/Logger.h>
#include <engine/util/Timer.h>
#include "util/GLUtil.h"
#include "engine/util/convert.h"
#include "../util/file.h"
#include "algorithm"

Texture2D::Texture2D(const std::string& path, Texture2DSettings settings) { load(path, settings); }

Texture2D::~Texture2D()
{
	free();
}

void Texture2D::free()
{
	if (isValid())
	{
		glDeleteTextures(1, &m_glId);
		m_glId = 0;
	}
}

void Texture2D::saveToFile(const std::string& filename)
{
	// Textures with one channel and uneven sizes are unsupported because
	// SOIL, stbi libs have a bug in the implementation apparently
	if (!isValid() || (m_channels == 1 && (m_width % 2 != 0 || m_height % 2 != 0)))
		return;

	unsigned char* pixels = new unsigned char[m_width * m_height * m_channels];

	bind();
	glGetTexImage(GL_TEXTURE_2D, 0, m_format, m_pixelType, pixels);

	// Invert Y
	for (int i = 0; i * 2 < m_height; ++i)
	{
		int idx0 = i * m_width * m_channels;
		int idx1 = (m_height - 1 - i) * m_width * m_channels;
		for (int j = m_width; j > 0; --j)
		{
			for (int ci = 0; ci < m_channels; ++ci)
			{
				std::swap(pixels[idx0 + ci], pixels[idx1 + ci]);
			}
	
			idx0 += m_channels;
			idx1 += m_channels;
		}
	}

	if (!SOIL_save_image(filename.c_str(), SOIL_SAVE_TYPE_PNG, m_width, m_height, m_channels, pixels))
	{
		LOG_ERROR("Failed to save Texture2D " << filename);
	}

	delete[] pixels;
}

void Texture2D::create(GLsizei width, GLsizei height, GLint internalFormat, GLenum format, GLenum pixelType, Texture2DSettings settings, const void* pixels)
{
    if (isValid())
    {
        glDeleteTextures(1, &m_glId);
        m_glId = 0;
    }

    m_target = GL_TEXTURE_2D;
    m_format = format;
    m_internalFormat = internalFormat;
    m_pixelType = pixelType;
    m_width = int(width);
    m_height = int(height);

    switch (format)
    {
    case GL_LUMINANCE:
    case GL_DEPTH_COMPONENT:
	case GL_RED:
	case GL_R:
        m_channels = 1;
        break;
    case GL_LUMINANCE_ALPHA:
	case GL_RG:
        m_channels = 2;
        break;
    case GL_RGB:
        m_channels = 3;
        break;
    case GL_RGBA:
        m_channels = 4;
        break;
    default: break;
    }

    glGenTextures(1, &m_glId);

    glBindTexture(m_target, m_glId);
    glTexImage2D(m_target, 0, m_internalFormat, m_width, m_height, 0, m_format, m_pixelType, pixels);

    applySettings(settings);
}

void Texture2D::createMultisampled(uint8_t numSamples, GLsizei width, GLsizei height, GLint internalFormat)
{
    assert(numSamples > 1);

    if (isValid())
    {
        glDeleteTextures(1, &m_glId);
        m_glId = 0;
    }

    m_target = GL_TEXTURE_2D_MULTISAMPLE;
    m_internalFormat = internalFormat;
    m_pixelType = 0;
    m_width = int(width);
    m_height = int(height);
    m_numSamples = numSamples;

    glGenTextures(1, &m_glId);

    glBindTexture(m_target, m_glId);
    glTexImage2DMultisample(m_target, GLsizei(numSamples), m_internalFormat, m_width, m_height, false);
}

void Texture2D::load(const std::string& path, Texture2DSettings settings, bool keepImageData)
{
	//LOG("Loading 2D texture " << path << " ...");

    m_target = GL_TEXTURE_2D;

    if (isValid())
    {
        glDeleteTextures(1, &m_glId);
        m_glId = 0;
    }

	std::string trimmedPath = path;
	trimmedPath.erase(std::remove(trimmedPath.begin(), trimmedPath.end(), '\t'), trimmedPath.end());

	m_glId = SOIL_load_OGL_texture(trimmedPath.c_str(), &m_width, &m_height, &m_channels, SOIL_LOAD_AUTO, SOIL_CREATE_NEW_ID, SOIL_FLAG_INVERT_Y);

	if (!m_glId)
	{
		LOG_ERROR("Failed to load: " + trimmedPath);
		return;
	}

    glBindTexture(m_target, m_glId);

    switch (m_channels)
    {
    case 1:
    {
        m_format = GL_RED;
        m_internalFormat = GL_R16;
        GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_ONE};
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        break;
    }
    case 2:
    {
        m_format = GL_RG;
        m_internalFormat = GL_RG16;
        GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_GREEN};
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);
        break;
    }
    case 3:
        m_format = GL_RGB;
        m_internalFormat = GL_RGB8;
        break;
    case 4:
        m_format = GL_RGBA;
        m_internalFormat = GL_RGBA8;
        break;
    default: break;
    }

    m_pixelType = GL_UNSIGNED_BYTE;

	try
	{
		applySettings(settings);
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		LOG_ERROR("Failed to load " << path);
	}

    GL_ERROR_CHECK();

	//LOG("Done.");
}

void Texture2D::setParameteri(GLenum name, GLint value) const
{
    assert(isValid());
    assert(GL::isTextureBound(m_target, m_glId));

    glTexParameteri(m_target, name, value);
}

void Texture2D::resize(GLsizei width, GLsizei height)
{
    m_width = width;
    m_height = height;

    glBindTexture(m_target, m_glId);
    if (m_target == GL_TEXTURE_2D)
        glTexImage2D(m_target, 0, m_internalFormat, m_width, m_height, 0, m_format, m_pixelType, nullptr);
    else
        glTexImage2DMultisample(m_target, GLsizei(m_numSamples), m_internalFormat, m_width, m_height, false);

    glBindTexture(m_target, 0);
}

void Texture2D::applySettings(Texture2DSettings settings)
{
    switch (settings)
    {
    case Texture2DSettings::S_T_REPEAT_MIN_MAG_LINEAR:
        glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;
    case Texture2DSettings::S_T_REPEAT_MIN_MIPMAP_LINEAR_MAG_LINEAR:
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		m_numMipmapLevels = 1 + static_cast<GLsizei>(std::floor(std::log2(std::max(m_width, m_height))));
        break;
    case Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MIPMAP_LINEAR_MAG_LINEAR:
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		m_numMipmapLevels = 1 + static_cast<GLsizei>(std::floor(std::log2(std::max(m_width, m_height))));
        break;
    case Texture2DSettings::S_T_REPEAT_MIN_MAG_NEAREST:
        glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        break;
    case Texture2DSettings::S_T_REPEAT_ANISOTROPIC:
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GLfloat fLargest;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &fLargest);
        glTexParameterf(m_target, GL_TEXTURE_MAX_ANISOTROPY_EXT, fLargest);
		m_numMipmapLevels = 1 + static_cast<GLsizei>(std::floor(std::log2(std::max(m_width, m_height))));
        break;
    case Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_NEAREST:
        glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        break;
    case Texture2DSettings::S_T_CLAMP_TO_BORDER_MIN_MAX_LINEAR:
        glTexParameteri(m_target, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(m_target, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(m_target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(m_target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        break;
    case Texture2DSettings::Custom:
        break;
    default:
        break;
    }
}
