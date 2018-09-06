#include "RTInteropTexture2D.h"
#include <CLW.h>
#include "../../../../../third_party/RadeonRays/CLW/CLWExcept.h"
#include "../../../../engine/util/Logger.h"

cl_mem RTInteropTexture2D::createFromOpenGLTexture(cl_context context, cl_mem_flags memFlags, std::shared_ptr<Texture2D> texture)
{
	m_glTexture = std::move(texture);
	m_clContext = context;
	m_memFlags = memFlags;
	cl_int status = CL_SUCCESS;
	m_clTexMem = clCreateFromGLTexture2D(context, memFlags, GL_TEXTURE_2D, 0, m_glTexture->getGLID(), &status);
	ThrowIf(status != CL_SUCCESS, status, "clCreateFromGLTexture failed");

	return m_clTexMem;
}

RTInteropTexture2D::~RTInteropTexture2D()
{
	if (m_clTexMem)
	{
		cl_int status = clReleaseMemObject(m_clTexMem);
		ThrowIf(status != CL_SUCCESS, status, "clReleaseMemObject failed");
	}
}

void RTInteropTexture2D::resize(int width, int height)
{
	assert(isValid());
	cl_int status;

	if (m_clTexMem)
	{
		status = clReleaseMemObject(m_clTexMem);
		ThrowIf(status != CL_SUCCESS, status, "clReleaseMemObject failed");
	}

	m_glTexture->resize(width, height);
	GL_ERROR_CHECK();

	status = CL_SUCCESS;
	m_clTexMem = clCreateFromGLTexture2D(m_clContext, m_memFlags, GL_TEXTURE_2D, 0, m_glTexture->getGLID(), &status);
	ThrowIf(status != CL_SUCCESS, status, "clCreateFromGLTexture failed");
}
