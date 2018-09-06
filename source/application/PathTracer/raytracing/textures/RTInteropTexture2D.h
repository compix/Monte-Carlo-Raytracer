#pragma once
#include <CLW.h>
#include "engine/rendering/Texture2D.h"
#include "memory"

class RTInteropTexture2D
{
public:
	cl_mem createFromOpenGLTexture(cl_context context, cl_mem_flags memFlags, std::shared_ptr<Texture2D> texture);

	~RTInteropTexture2D();

	void resize(int width, int height);

	bool isValid() const { return m_clTexMem != 0 && m_glTexture && m_glTexture->isValid(); }
	cl_mem getCLMem() const { return m_clTexMem; }
	std::shared_ptr<Texture2D> getGLTexture() const { return m_glTexture; }
protected:
	cl_mem m_clTexMem = 0;
	std::shared_ptr<Texture2D> m_glTexture;
	cl_context m_clContext;
	cl_mem_flags m_memFlags;
};