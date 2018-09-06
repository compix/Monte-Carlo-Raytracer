#pragma once
#include "radeon_rays.h"
#include <glm/glm.hpp>

namespace CLHelper
{
	inline RadeonRays::float2 toFloat2(const glm::vec2& vec)
	{
		return RadeonRays::float2(vec.x, vec.y);
	}

	inline RadeonRays::float3 toFloat3(const glm::vec3& vec)
	{
		return RadeonRays::float3(vec.x, vec.y, vec.z);
	}

	inline RadeonRays::float4 toFloat4(const glm::vec4& vec)
	{
		return RadeonRays::float4(vec.x, vec.y, vec.z, vec.w);
	}

	inline RadeonRays::matrix toMatrix(const glm::mat4& mat)
	{
		// RadeonRays::matrix is row major, glm::mat4 is column major and thus needs to be transposed.
		return RadeonRays::matrix(
			mat[0][0], mat[1][0], mat[2][0], mat[3][0],
			mat[0][1], mat[1][1], mat[2][1], mat[3][1],
			mat[0][2], mat[1][2], mat[2][2], mat[3][2],
			mat[0][3], mat[1][3], mat[2][3], mat[3][3]);
	}

	template<class T>
	CLWBuffer<T> createBuffer(cl_context context, cl_mem_flags flags, size_t elementCount, size_t& outUsedMemory, void* data = nullptr)
	{
		outUsedMemory = sizeof(T) * elementCount;
		if (data)
			return CLWBuffer<T>::Create(context, flags, elementCount, data);
		else
			return CLWBuffer<T>::Create(context, flags, elementCount);
	}
}
