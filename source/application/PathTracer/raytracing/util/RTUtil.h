#pragma once
#include <engine/geometry/Ray.h>

namespace RTUtil
{
	/**
	* pixelOffset is jittering for TAA.
	*/	
	Ray screenToRay(const glm::vec3& p);
}