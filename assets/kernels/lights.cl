#ifndef LIGHTS_CL
#define LIGHTS_CL

#define SPEED_OF_LIGHT 299792458
#define PLANCKS_CONSTANT 6.62606957e-34
#define BOLTZMANN_CONSTANT 1.3806488e-23

#include <samplers.cl>
#include <math.cl>
#include <kernel_data.h>

#define RT_DIRECTIONAL_LIGHT_TRACE_DISTANCE 1000.0f

inline bool isDeltaLight(int flags)
{ 
	return flags & RT_LIGHT_FLAG_DELTA_LIGHT;
}

float3 computeTotalPower_PointLight(float3 intensity)
{ 
	return 4.0f * PI * intensity;
}

inline float3 evalDiffuseAreaLightL(float3 intensity, const RTInteraction* interaction, float3 w)
{ 
	return dot(interaction->gn, w) > 0.0f ? intensity : (float3)(0.0f);
}

float3 evalLightLe(__global const RTLight* light, float3 gn, float3 w)
{ 
	switch(light->type)
	{ 
		case RT_DISK_AREA_LIGHT:
		case RT_TRIANGLE_MESH_AREA_LIGHT:
			return dot(gn, w) >  0.0f ? light->intensity : (float3)(0.0f);
		default:
			return (float3)(0.0f);
	}
}

/**
* This implementation depends on the given light type.
* @param u Sample in [0,1]^3
*/
float3 sampleLightLi(int lightIdx, const Scene* scene, const RTInteraction* interaction, float2 u, float3* lightPosition, float3* lightNormal, float3* wi, float* pdf, __global RTRay* shadowRay)
{ 
	__global const RTLight* light = scene->lights + lightIdx;

	switch(light->type)
	{ 
		case RT_DIRECTIONAL_LIGHT:
		{ 
			*wi = -light->d;
			*lightNormal = (float3)(0.0f);
			*lightPosition = interaction->p + *wi * light->radius * 2.0f;
			*pdf = 1.0f;
			setRay(shadowRay, interaction->p + interaction->gn * interaction->traceErrorOffset, RT_DIRECTIONAL_LIGHT_TRACE_DISTANCE, *wi);
			return light->intensity;
		}
		case RT_POINT_LIGHT:
		{
			*wi = light->p - interaction->p;
			float distSq = dot(*wi, *wi);
			if (isNearZero(distSq))
				return (float3)(0.0f);
			
			float dist = sqrt(distSq);
			*wi /= dist;
			*lightNormal = (float3)(0.0f);
			*pdf = 1.0f;
			float3 rayOrigin = interaction->p + interaction->gn * interaction->traceErrorOffset;
			*lightPosition = light->p;
			setRay(shadowRay, rayOrigin, dist, *wi);
			return light->intensity / distSq;
		}
		case RT_DISK_AREA_LIGHT:
		{ 
			RTInteraction shapeInter = sampleDisk(light->p, light->d, light->radius, u.xy, pdf);
			*lightNormal = shapeInter.gn;
			*lightPosition = shapeInter.p;

			float3 rayOrigin = interaction->p + interaction->gn * interaction->traceErrorOffset;
			float3 rayTarget = shapeInter.p + shapeInter.gn * RT_TRACE_OFFSET;
			*wi = normalize(rayTarget - rayOrigin);

			float distSq = distanceSquared(shapeInter.p, interaction->p);
			float c = absDot(shapeInter.gn, -*wi);
			if (isNearZero(c))
			{ 
				*pdf = 0.0f;
				return (float3)(0.0f);
			}
			else
			{
				// Convert to density with respect to solid angle.
				*pdf *= distSq / c;
			}

			setRay(shadowRay, rayOrigin, distance(rayOrigin, rayTarget), *wi);
			return evalDiffuseAreaLightL(light->intensity, &shapeInter, -*wi);
		}
		case RT_TRIANGLE_MESH_AREA_LIGHT:
		{
			RTShape shape = scene->shapes[light->shapeId];
			// Use u.x to choose a random triangle.
			int triangleIdx = ((int)floor(u.x * shape.numTriangles)) % shape.numTriangles;

			// Remap u.x for correct distribution.
			u.x = u.x * shape.numTriangles - triangleIdx;

			unsigned int i0 = scene->indices[shape.startIdx + 3 * triangleIdx];
			unsigned int i1 = scene->indices[shape.startIdx + 3 * triangleIdx + 1];
			unsigned int i2 = scene->indices[shape.startIdx + 3 * triangleIdx + 2];

			float3 p0 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex + i0]);
			float3 p1 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex + i1]);
			float3 p2 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex + i2]);

			RTInteraction shapeInter = sampleTriangle(p0, p1, p2, u.xy, pdf);
			*lightNormal = shapeInter.gn;
			*lightPosition = shapeInter.p;
			*pdf = 1.0f / light->area;

			float3 rayOrigin = interaction->p + interaction->gn * interaction->traceErrorOffset;
			float3 rayTarget = shapeInter.p + shapeInter.gn * RT_TRACE_OFFSET;
			*wi = normalize(shapeInter.p - interaction->p);

			float distSq = distanceSquared(shapeInter.p, interaction->p);
			float c = absDot(shapeInter.gn, -*wi);
			if (isNearZero(c))
			{ 
				*pdf = 0.0f;
				return (float3)(0.0f);
			}
			else
			{
				*pdf *= distSq / c;
			}

			setRay(shadowRay, rayOrigin, distance(rayOrigin, rayTarget), *wi);
			return evalDiffuseAreaLightL(light->intensity, &shapeInter, -*wi);
		}
		default:
		return (float3)(0.0f);
	}
}

float3 sampleLightLe(int lightIdx, const Scene* scene, float2 u1, float2 u2, float3* rayOrigin, float3* rayDirection, float3* lightNormal, float* pdfPos, float* pdfDir)
{ 
	__global const RTLight* light = scene->lights + lightIdx;

	switch(light->type)
	{ 
		case RT_DIRECTIONAL_LIGHT:
		{ 
			// Note: The radius of the light must be set to the radius of the world
			//       and the the position to worldCenter - light->d * worldRadius. 
			RTInteraction shapeInter = sampleDisk(light->p, light->d, light->radius, u1, pdfPos);
			*lightNormal = light->d;
			*pdfDir = 1.0f;
			*rayOrigin = shapeInter.p;
			*rayDirection = light->d;

			return light->intensity;
		}
		case RT_POINT_LIGHT:
		{
			*rayDirection = uniformSampleSphere(u1);
			*rayOrigin = light->p;
			*lightNormal = *rayDirection;
			*pdfPos = 1.0f;
			*pdfDir = uniformSpherePdf();
			return light->intensity;
		}
		case RT_DISK_AREA_LIGHT:
		{ 
			// Sample a point on disk
			RTInteraction shapeInter = sampleDisk(light->p, light->d, light->radius, u1, pdfPos);
			*lightNormal = shapeInter.gn;
			float3 w = cosineSampleHemisphere(u2);
			*pdfDir = cosineHemispherePdf(w.y);
			// Transform direction to world space
			float3 v0 = computeOrthogonalVector(shapeInter.gn);
			float3 v1 = cross(v0, shapeInter.gn);
			*rayDirection = w.x * v0 + w.y * shapeInter.gn + w.z * v1;
			*rayOrigin = shapeInter.p + shapeInter.gn * RT_TRACE_OFFSET;

			return light->intensity;
		}
		case RT_TRIANGLE_MESH_AREA_LIGHT:
		{
			RTShape shape = scene->shapes[light->shapeId];
			// Use u1.x to choose a random triangle.
			int triangleIdx = ((int)floor(u1.x * shape.numTriangles)) % shape.numTriangles;

			// Remap u1.x for correct distribution.
			u1.x = u1.x * shape.numTriangles - triangleIdx;

			const uint i0 = scene->indices[shape.startIdx + 3 * triangleIdx];
			const uint i1 = scene->indices[shape.startIdx + 3 * triangleIdx + 1];
			const uint i2 = scene->indices[shape.startIdx + 3 * triangleIdx + 2];

			const float3 p0 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex + i0]);
			const float3 p1 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex + i1]);
			const float3 p2 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex + i2]);

			RTInteraction shapeInter = sampleTriangle(p0, p1, p2, u1, pdfPos);
			*pdfPos = 1.0f / light->area;
			*lightNormal = shapeInter.gn;
			float3 w = cosineSampleHemisphere(u2);
			*pdfDir = cosineHemispherePdf(w.y);

			// Transform direction to world
			float3 v0 = computeOrthogonalVector(shapeInter.gn);
			float3 v1 = cross(v0, shapeInter.gn);
			*rayDirection = w.x * v0 + w.y * shapeInter.gn + w.z * v1;
			*rayOrigin = shapeInter.p + shapeInter.gn * RT_TRACE_OFFSET;

			return light->intensity;
		}
		default:
		return (float3)(0.0f);
	}
}

void evalLightPdfLe(int lightIdx, const Scene* scene, float3 rayDirection, float3 lightNormal, float* pdfPos, float* pdfDir)
{ 
	__global const RTLight* light = scene->lights + lightIdx;

	switch(light->type)
	{ 
		case RT_DIRECTIONAL_LIGHT:
		{ 
			*pdfPos = 1.0f / light->area;
			*pdfDir = 0.0f;
		}
		break;
		case RT_POINT_LIGHT:
		{
			*pdfPos = 0.0f;
			*pdfDir = uniformSpherePdf();
		}
		break;
		case RT_DISK_AREA_LIGHT:
		case RT_TRIANGLE_MESH_AREA_LIGHT:
		{ 
			*pdfPos = 1.0f / light->area;
			*pdfDir = cosineHemispherePdf(dot(lightNormal, rayDirection));
		}
		break;
	}
}


#endif