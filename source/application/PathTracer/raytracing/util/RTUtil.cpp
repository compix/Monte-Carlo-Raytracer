#include "RTUtil.h"
#include "../../GUI/PathTracingSettings.h"
#include "../../../../engine/globals.h"
#include "../../../../engine/geometry/Ray.h"
#include "../../../../engine/camera/CameraComponent.h"
#include "../../../../engine/util/Random.h"
#include "../../../../engine/camera/FreeCameraViewController.h"

Ray RTUtil::screenToRay(const glm::vec3& p)
{

	// Convert to NDC space
	glm::vec3 ndcP = MainCamera->screenToNDC(p);
	glm::vec4 start(ndcP, 1.f);
	glm::vec4 end(ndcP.x, ndcP.y, 1.0f, 1.f);

	// Jitter for TAA
	//float wi = 0.0f;
	//float hi = 0.0f;
	//if (PathTracerSettings::GI.useTAA && !MainCamera->getComponent<FreeCameraViewController>()->bMovedInLastUpdate)
	//{
	//	wi = 2.0f / width;
	//	hi = 2.0f / height;
	//}
	//glm::mat4 inv = glm::inverse(glm::translate(glm::vec3(Random::getFloat(-wi, wi), Random::getFloat(-hi, hi), 0.0f)) * MainCamera->viewProj());

	glm::vec2 pixelOffset = PathTracerSettings::GI.filterSettings.curPixelOffset;
	// Normalize pixel offset:
	pixelOffset.x /= Screen::getWidth();
	pixelOffset.y /= Screen::getHeight();
	glm::mat4 inv = glm::inverse(glm::translate(glm::vec3(pixelOffset.x, pixelOffset.y, 0.0f)) * MainCamera->viewProj());

	// Convert to world space
	start = inv * start;
	start /= start.w;

	end = inv * end;
	end /= end.w;

	return Ray(glm::vec3(start), glm::normalize(glm::vec3(end - start)));
}
