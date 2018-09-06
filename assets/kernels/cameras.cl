#ifndef CAMERAS_CL
#define CAMERAS_CL

#include <math.cl>
#include <kernel_data.h>
#include <matrix.cl>

float3 evalPinholeCameraWe(__global const RTPinholeCamera* camera, float3 rayOrigin, float3 rayDirection, float2* normalizedImagePos)
{ 
	float cosTheta = dot(rayDirection, camera->direction);

	if (cosTheta <= 0.0f)
		return (float3)(0.0f, 0.0f, 0.0f);

	float4 pFocus;
	pFocus.xyz = rayOrigin + rayDirection / cosTheta;
	pFocus.w = 1.0f;

	float4 imgClip = transformVector4(camera->worldToClip, pFocus);

	// Check if point is out of bounds in clip space
	if (imgClip.x < -imgClip.w || imgClip.x > imgClip.w || imgClip.y < -imgClip.w || imgClip.y > imgClip.w)
		return (float3)(0.0f);

	// Compute normalized image position in [0,1]^2 if requested
	if (normalizedImagePos)
	{
		*normalizedImagePos = (imgClip.xy / imgClip.w + (float2)(1.0f)) * 0.5f;
	}

	return (float3)(1.0f / (camera->area * cosTheta * cosTheta * cosTheta));
}

void evalPinholeCameraPdfWe(__global const RTPinholeCamera* camera, float3 rayOrigin, float3 rayDirection, float* pdfPos, float* pdfDir)
{
	float cosTheta = dot(rayDirection, camera->direction);

	if (cosTheta <= 0.0f)
	{
		*pdfPos = 0.0f;
		*pdfDir = 0.0f;
		return;
	}

	float4 pFocus;
	pFocus.xyz = rayOrigin + rayDirection * (1.0f / cosTheta);
	pFocus.w = 1.0f;

	float4 imgClip = transformVector4(camera->worldToClip, pFocus);

	// Check if point is out of bounds in clip space
	if (imgClip.x < -imgClip.w || imgClip.x > imgClip.w || imgClip.y < -imgClip.w || imgClip.y > imgClip.w)
	{
		*pdfPos = 0.0f;
		*pdfDir = 0.0f;
		return;
	}

	*pdfPos = 1.0f;
	*pdfDir = 1.0f / (camera->area * cosTheta * cosTheta * cosTheta);
}

float3 samplePinholeCameraWi(__global const RTPinholeCamera* camera, const RTInteraction* si, float3* wi, float* pdf, float2* normalizedImagePos)
{
	*wi = camera->pos - si->p;
	float dist = length(*wi);
	*wi /= dist;

	*pdf = (dist * dist) / absDot(camera->direction, *wi);
	return evalPinholeCameraWe(camera, camera->pos, -*wi, normalizedImagePos);
}

#endif // CAMERAS_CL