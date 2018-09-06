#ifndef BIDIRECTIONAL_INTEGRATOR_CL
#define BIDIRECTIONAL_INTEGRATOR_CL

#include "rng.cl"
#include "kernel_data.h"
#include "samplers.cl"
#include "bxdfs.cl"
#include "materials.cl"
#include "geometry.cl"
#include "lights.cl"
#include "cameras.cl"
#include "image_samplers.cl"

// The implementation is based on the content of the excellent book "Physically Based Rendering From Theory to Implementation Third Edition".

typedef RTBDPTVertex Vertex;

//#define SHOW_REGULAR_PATH_TRACER_RESULTS

/**
* When using importance transport mode it is necessary to apply a correction for shading normals.
*/
float computeShadingNormalCorrection(const RTInteraction* inter, float3 wo, float3 wi, TransportMode mode)
{ 
	if (mode == TRANSPORT_MODE_IMPORTANCE)
	{ 
		float denom = absDot(wo, inter->gn) * absDot(wi, inter->sn);

		if (isNearZero(denom))
			return 0.0f;

		return (absDot(wo, inter->sn) * absDot(wi, inter->gn)) / denom;
	}
	else
		return 1.0f;
}

// ************************************ Vertex defines
inline bool isVertexOnSurface(__global const RTBDPTVertex* vertex)
{
	return isNotNearZero(vertex->interaction.gn.x) || isNotNearZero(vertex->interaction.gn.y) || isNotNearZero(vertex->interaction.gn.z);
}

inline float convertVertexDensity(float pdf, __global const RTBDPTVertex* thisVertex, __global const RTBDPTVertex* nextVertex)
{
	if (isVertexInfiniteLight(nextVertex))
		return pdf;
	
	float3 w = nextVertex->interaction.p - thisVertex->interaction.p;
	float lenSq = dot(w, w);
	if (isNearZero(lenSq))
		return 0.0f;

	float invDistSq = 1.0f / lenSq;

	if (isVertexOnSurface(nextVertex))
		pdf *= absDot(nextVertex->interaction.gn, w * sqrt(invDistSq));

	return pdf * invDistSq;
}

float evalVertexPdfLight(const Scene* scene, __global const Vertex* thisVertex, __global const Vertex* v)
{
	float3 w = v->interaction.p - thisVertex->interaction.p;
	float lenSq = dot(w, w);
	if (isNearZero(lenSq))
		return 0.0f;

	float invDistSq = 1.0f / lenSq;
	w *= sqrt(invDistSq);

	float pdf;
	if (isVertexInfiniteLight(thisVertex))
	{ 
		float radius = scene->lights[thisVertex->lightIdx].radius;
		pdf = 1.0f / (PI * radius * radius);
	}
	else
	{ 
		float pdfPos;
		float pdfDir;
		evalLightPdfLe(thisVertex->lightIdx, scene, w, thisVertex->interaction.gn, &pdfPos, &pdfDir);
		pdf = pdfDir * invDistSq;
	}

	if (isVertexOnSurface(v))
		pdf *= absDot(v->interaction.gn, w);

	return pdf;
}

float evalVertexPdf(const Scene* scene, __global const Vertex* thisVertex, __global const Vertex* prevVertex, __global const Vertex* nextVertex)
{
	if (thisVertex->type == RT_BDPT_LIGHT_VERTEX)
		return evalVertexPdfLight(scene, thisVertex, nextVertex);

	// Compute directions for next and prev vertices
	float3 wn = nextVertex->interaction.p - thisVertex->interaction.p;
	float lenSq = dot(wn, wn);

	if (isNearZero(lenSq))
		return 0.0f;

	wn /= sqrt(lenSq);

	float pdf;
	float unused;

	if (thisVertex->type == RT_BDPT_CAMERA_VERTEX)
	{
		evalPinholeCameraPdfWe(scene->camera, thisVertex->interaction.p, wn, &unused, &pdf);
	}
	else if (thisVertex->type == RT_BDPT_SURFACE_VERTEX)
	{
		float3 wp = (float3)(0.0f);

		if (prevVertex)
		{ 
			wp = prevVertex->interaction.p - thisVertex->interaction.p;
			lenSq = dot(wp, wp);
			if (isNearZero(lenSq))
				return 0.0f;

			wp /= sqrt(lenSq);
		}
		RTInteraction inter = thisVertex->interaction;
		pdf = evaluateMaterialPdf(scene, thisVertex->materialIdx, wp, wn, &inter, BSDF_ALL);
	}

	// Convert solid angle density to per unit area density.
	return convertVertexDensity(pdf, thisVertex, nextVertex);
}


float evalVertexPdfLightOrigin(const Scene* scene, __global const Vertex* thisVertex, float3 nextVertexPos)
{ 
	float3 w = nextVertexPos - thisVertex->interaction.p;
	float lenSq = dot(w, w);
	if (isNearZero(lenSq))
		return 0.0f;
		
	w *= sqrt(1.0f / lenSq);

	if (isVertexInfiniteLight(thisVertex))
	{ 
		return 0.0f;
	}

	float pdfPos;
	float pdfDir;

	evalLightPdfLe(thisVertex->lightIdx, scene, w, thisVertex->interaction.gn, &pdfPos, &pdfDir);
	return pdfPos * scene->lights[thisVertex->lightIdx].choicePdf;
}


// Vertex creation functions

inline RTBDPTVertex createCameraVertex(float3 p, float3 throughput)
{
	Vertex v;
	v.throughput = throughput;
	RTInteraction interaction;
	interaction.p = p;
	interaction.gn = (float3)(0.0f);
	v.interaction = interaction;
	v.type = RT_BDPT_CAMERA_VERTEX;
	v.flags = RT_BDPT_VERTEX_FLAG_CONNECTIBLE;
	v.lightIdx = RT_INVALID_ID;
	v.pdfRev = 0.0f;
	v.pdfFwd = 0.0f;

	return v;
}

inline RTBDPTVertex createLightVertex(int lightIdx, float3 p, float3 lightNormal, float3 throughput, float pdfFwd, int lightFlags)
{
	Vertex v;
	v.throughput = throughput;
	RTInteraction interaction;
	interaction.p = p;
	interaction.gn = lightNormal;
	interaction.sn = lightNormal;
	interaction.traceErrorOffset = RT_TRACE_OFFSET;
	v.interaction = interaction;
	v.type = RT_BDPT_LIGHT_VERTEX;
	v.lightIdx = lightIdx;
	v.pdfRev = 0.0f;
	v.pdfFwd = pdfFwd;

	if ((lightFlags & RT_LIGHT_FLAG_DELTA_DIRECTION) != 0)
		v.flags = RT_BDPT_VERTEX_FLAG_DELTA_LIGHT | RT_BDPT_VERTEX_FLAG_INFINITE_LIGHT;
	else	
	{ 
		v.flags = RT_BDPT_VERTEX_FLAG_CONNECTIBLE;

		if ((lightFlags & RT_LIGHT_FLAG_DELTA_POSITION) != 0)
			v.flags |= RT_BDPT_VERTEX_FLAG_DELTA_LIGHT;
	}

	return v;
}

inline void setSurfaceVertex(__global Vertex* thisVertex, RTInteraction interaction, int materialIdx, float3 throughput, float pdfFwd, __global const Vertex* prevVertex)
{
	thisVertex->throughput = throughput;
	thisVertex->interaction = interaction;
	thisVertex->type = RT_BDPT_SURFACE_VERTEX;
	thisVertex->materialIdx = materialIdx;
	thisVertex->flags = 0;
	thisVertex->pdfRev = 0.0f;
	thisVertex->pdfFwd = convertVertexDensity(pdfFwd, prevVertex, thisVertex);
}

inline float3 evalVertex_f(const Scene* scene, __global const Vertex* thisVertex, __global const Vertex* nextVertex, TransportMode mode)
{ 
	float3 wi = nextVertex->interaction.p - thisVertex->interaction.p;
	float lenSq = dot(wi, wi);

	if (isNearZero(lenSq))
		return (float3)(0.0f);

	wi /= sqrt(lenSq);

	if (thisVertex->type == RT_BDPT_SURFACE_VERTEX)
	{ 
		RTInteraction si = thisVertex->interaction;
		float3 f = evaluateMaterial(scene, thisVertex->materialIdx, si.wo, wi, &si, mode);

		return f * computeShadingNormalCorrection(&si, si.wo, wi, mode);
	}

	// Shouldn't happen, output pink color
	return (float3)(1.0f, 0.0784f, 0.5765f);
}

// ************************************ End vertex defines


__kernel void GenerateStartVertices(
									SCENE_PARAMS,
									IMAGE_PARAMS,
									int integrator_frameNum,
									int maxDepth,
								    __global RTBDPTVertex* restrict cameraVertices,
								    __global RTRay* restrict cameraRays,
									__global float3* restrict cameraThroughputs,
									__global float* restrict cameraFwdPdfs,
									__global int* restrict cameraVertexCounts,

									__global RTBDPTVertex* restrict lightVertices,
									__global RTRay* restrict lightRays,
									__global float3* restrict lightThroughputs,
									__global float* restrict lightFwdPdfs,
									__global int* restrict lightVertexCounts,
									__global float* restrict finalRadianceBuffer)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

	if (gid.x >= image_width || gid.y >= image_height)
		return;

	const int bufferIdx = gid.x + gid.y * image_width;
	const int cameraVertexIdx = bufferIdx * (maxDepth + 2);
	const int lightVertexIdx = bufferIdx * (maxDepth + 1);

	__global const RTPinholeCamera* camera = scene_camera;

	const int radianceBufferIdx = bufferIdx * 3;
	finalRadianceBuffer[radianceBufferIdx] = 0.0f;
	finalRadianceBuffer[radianceBufferIdx + 1] = 0.0f;
	finalRadianceBuffer[radianceBufferIdx + 2] = 0.0f;

	MAKE_SCENE(scene);
	MAKE_SAMPLER(sampler, bufferIdx, 0);

	cameraVertexCounts[bufferIdx] = 1;
	lightVertexCounts[bufferIdx] = 1;

	// ***** Handle Camera *****
	// Set camera ray
	const float2 r = (float2)(1.0f / image_width, 1.0f / image_height);
	const float2 uv = (float2)(gid.x * r.x, gid.y * r.y);
	setRay(cameraRays + bufferIdx, camera->pos, RT_MAX_TRACE_DISTANCE, lerpDirection(camera->r00, camera->r10, camera->r11, camera->r01, uv.x, uv.y));

	// Set initial camera vertex
	cameraVertices[cameraVertexIdx] = createCameraVertex(camera->pos, (float3)(1.0f));
	cameraThroughputs[bufferIdx] = cameraVertices[cameraVertexIdx].throughput;
	float pdfPos;
	float pdfDir;
	evalPinholeCameraPdfWe(camera, camera->pos, cameraRays[bufferIdx].d.xyz, &pdfPos, &pdfDir); 
	cameraFwdPdfs[bufferIdx] = pdfDir;

	// ***** Handle Light *****
	// Set light ray
	int chosenLightIdx = ((uint)floor(getSample1D(&sampler) * scene.numLights)) % scene.numLights;
	float lightPdf = scene.lights[chosenLightIdx].choicePdf;
	float2 u1 = getSample2D(&sampler);
	float2 u2 = getSample2D(&sampler);
	float3 rayOrigin;
	float3 rayDirection;
	float3 lightNormal;
	float3 Le = sampleLightLe(chosenLightIdx, &scene, u1, u2, &rayOrigin, &rayDirection, &lightNormal, &pdfPos, &pdfDir);
	setRay(lightRays + bufferIdx, rayOrigin, RT_MAX_TRACE_DISTANCE, rayDirection);

	// Set light vertex
	lightVertices[lightVertexIdx] = createLightVertex(chosenLightIdx, rayOrigin, lightNormal, Le, pdfPos * lightPdf, scene.lights[chosenLightIdx].flags);
	lightVertices[lightVertexIdx].pdfPos = pdfPos;

	lightThroughputs[bufferIdx] = Le * absDot(lightNormal, rayDirection) / (lightPdf * pdfPos * pdfDir);
	lightFwdPdfs[bufferIdx] = pdfDir;
}

/**
* Note: If isCameraPath = true then TransportMode is Radiance otherwise Importance.
*/
__kernel void GenerateSecondaryVertices(SCENE_PARAMS,
								   IMAGE_PARAMS,
								   int integrator_frameNum,
								   int maxDepth,
								   int isCameraPath,
								   __global RTBDPTVertex* restrict vertices,
								   __global RTRay* restrict rays,
								   __global const RTIntersection* restrict intersections,
								   __global float3* restrict throughputs,
								   __global float* restrict fwdPdfs,
								   __global int* restrict vertexCounts,
								   int curDepth)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

	if (gid.x >= image_width || gid.y >= image_height)
		return;

	const int bufferIdx = gid.x + gid.y * image_width;
	const int vertexIdx = bufferIdx * (maxDepth + 1 + isCameraPath) + curDepth;

	MAKE_SCENE(scene);
	MAKE_SAMPLER(sampler, bufferIdx, curDepth + (maxDepth + 1) * isCameraPath);

	RTIntersection isect = intersections[bufferIdx];
	int shapeIdx = isect.shapeid;
    int primitiveIdx = isect.primid;

#ifdef SHOW_REGULAR_PATH_TRACER_RESULTS
	if (!isCameraPath)
	{ 
		setRayInactive(rays + bufferIdx);
		return;
	}
#endif

	// Handle case where ray didn't hit anything or is inactive
    if (isRayInactive(rays + bufferIdx) || shapeIdx == RT_INVALID_ID || primitiveIdx == RT_INVALID_ID || scene.shapes[shapeIdx].materialId == RT_INVALID_ID)
	{ 
		setRayInactive(rays + bufferIdx);
		return;
	}
	// There was an intersection - handle surface interaction (medium interactions not yet supported)
	else
	{
		vertexCounts[bufferIdx]++;

		// Initialize RTInteraction:
		RTInteraction si = computeSurfaceInteraction(&scene, &isect);
		si.wo = -rays[bufferIdx].d.xyz;
		const bool isBackfacing = dot(si.gn, si.wo) < 0.0f;
		si.traceErrorOffset = isBackfacing ? -RT_TRACE_OFFSET : RT_TRACE_OFFSET;
		int materialIdx = scene.shapes[shapeIdx].materialId;
		applyNormalMapping(&scene, materialIdx, &si);

		// Create vertex:
		__global Vertex* curVertex = vertices + vertexIdx;
		__global Vertex* prevVertex = vertices + vertexIdx - 1;

		TransportMode transportMode = isCameraPath ? TRANSPORT_MODE_RADIANCE : TRANSPORT_MODE_IMPORTANCE;

		float pdfFwd = fwdPdfs[bufferIdx];
		setSurfaceVertex(curVertex, si, materialIdx, throughputs[bufferIdx], pdfFwd, prevVertex);
		// It's possible that this vertex is also an area light
		curVertex->lightIdx = scene.shapes[shapeIdx].lightID;

		// Subpath correction for infinite lights
		if (!isCameraPath && curDepth == 1 && isVertexInfiniteLight(prevVertex))
		{
			curVertex->pdfFwd = prevVertex->pdfPos;
			if (isVertexOnSurface(curVertex))
			{ 
				curVertex->pdfFwd *= absDot(rays[bufferIdx].d.xyz, si.gn);
			}

			prevVertex->pdfFwd = 0.0f;
		}

		// No need to compute the next bounce if maxDepth was reached
		if (curDepth == (maxDepth + isCameraPath))
		{
			if (hasMaterialNonDeltaComponents(&scene, materialIdx, &si))
				curVertex->flags |= RT_BDPT_VERTEX_FLAG_CONNECTIBLE;

			setRayInactive(rays + bufferIdx);
			return;
		}

		// Compute next bounce by sampling the bsdf at the current interaction
		float3 wi;
		float3 wo = si.wo;
		
		BxDFType sampledType;
		int numNonDeltaTypes;
		float2 bsdfSample = getSample2D(&sampler);
		float3 f = sampleMaterial(&scene, materialIdx, &si, bsdfSample, transportMode, BSDF_ALL, wo, &wi, &pdfFwd, &numNonDeltaTypes, &sampledType);
		curVertex->interaction = si;
		
		if (numNonDeltaTypes > 0)
			curVertex->flags |= RT_BDPT_VERTEX_FLAG_CONNECTIBLE;

		// Done if bsdf is black or pdf is zero
		if (isBlack(f) || isNearZero(pdfFwd))
		{ 
			setRayInactive(rays + bufferIdx);
			return;
		}

		// Update throughput
		throughputs[bufferIdx] *= f * absDot(wi, si.sn) / pdfFwd;
		float pdfRev;

		// If a specular bsdf was sampled store this information in vertex and adjust pdfs for delta distribution.
		if ((sampledType & BSDF_SPECULAR) != 0)
		{ 
			curVertex->flags |= RT_BDPT_VERTEX_FLAG_DELTA;
			pdfFwd = 0.0f;
			pdfRev = 0.0f;
		}
		else
		{ 
			// Compute reverse pdf by swapping wo, wi
			pdfRev = evaluateMaterialPdf(&scene, materialIdx, wi, wo, &si, BSDF_ALL);
		}

		// Set ray for next bounce
		float traceErrorOffset = si.traceErrorOffset;

		// Check if a transmissive bsdf was sampled.
		if ((sampledType & BSDF_TRANSMISSION) != 0 && dot(si.gn, wi) * sign(traceErrorOffset) < 0.0f)
			traceErrorOffset *= -1.0f;
		setRay(rays + bufferIdx, si.p + si.gn * traceErrorOffset, RT_MAX_TRACE_DISTANCE, wi);

		// Shading normals are asymetric for camera, light paths -> compute correction
		throughputs[bufferIdx] *= computeShadingNormalCorrection(&si, wo, wi, transportMode);

		// Compute reverse pdf for previous vertex
		prevVertex->pdfRev = convertVertexDensity(pdfRev, curVertex, prevVertex);

		fwdPdfs[bufferIdx] = pdfFwd;
	}
}

__kernel void PrepareConnections(SCENE_PARAMS,
							IMAGE_PARAMS,
							int integrator_frameNum,
							int maxDepth,
							__global RTBDPTVertex* restrict cameraVertices,
							__global RTBDPTVertex* restrict lightVertices,
							__global RTBDPTVertex* restrict sampledCameraVertices,
							__global RTBDPTVertex* restrict sampledLightVertices,
							__global RTRay* restrict connectionRays,
							__global const int* restrict cameraVertexCounts,
							__global const int* restrict lightVertexCounts,
							__global float4* radianceBuffer)
{
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

	if (gid.x >= image_width || gid.y >= image_height)
		return;

	const int maxLightVertices = maxDepth + 1;
	const int maxCameraVertices = maxDepth + 2;

	const int bufferIdx = gid.x + gid.y * image_width;

	const int cameraVertexCount = cameraVertexCounts[bufferIdx];
	const int lightVertexCount = lightVertexCounts[bufferIdx];

	const int camVertexStartIdx = bufferIdx * maxCameraVertices;
	const int lightVertexStartIdx = bufferIdx * maxLightVertices;

	const int maxConnections = maxCameraVertices * (maxCameraVertices + 1) / 2 - 2;
	int curConnectionRayIdx = bufferIdx * maxConnections;

	MAKE_SCENE(scene);
	MAKE_SAMPLER(sampler, bufferIdx, maxLightVertices + maxCameraVertices);

	// t is the current number of camera vertices.
	// s is the current number of light vertices.
	for (int t = 1; t <= cameraVertexCount; ++t)
	{ 
		for (int s = 0; s <= lightVertexCount; ++s)
		{ 
			int curDepth = t + s - 2;
			if ((t == 1 && s == 1) || curDepth < 0 || curDepth > maxDepth)
			{ 
				continue;
			}

#ifdef SHOW_REGULAR_PATH_TRACER_RESULTS
			if (s != 1) continue;
#endif

			__global Vertex* cameraVertex = cameraVertices + camVertexStartIdx + t - 1;

			radianceBuffer[curConnectionRayIdx].xyz = (float3)(0.0f);

			if (s == 0)
			{ 
				// Do nothing here
			}
			else if (t == 1)
			{ 
				// Connect a sampled camera point to light subpath
				__global Vertex* lightVertex = lightVertices + lightVertexStartIdx + s - 1;

				if (isVertexConnectible(lightVertex))
				{
					RTInteraction lightInter = lightVertex->interaction;
					float3 wi;
					float pdf;
					float2 normalizedImagePos = (float2)((float)(gid.x) / image_width, (float)(gid.y) / image_height);
					float3 importance = samplePinholeCameraWi(scene_camera, &lightInter, &wi, &pdf, &normalizedImagePos);
					if (pdf > 0.0f && isNotBlack(importance))
					{
						// Note: Min value for s must be 2 here since t == 1
						const int sampledCameraVertexIdx = bufferIdx * maxDepth + s - 2;
						__global Vertex* sampled = sampledCameraVertices + sampledCameraVertexIdx;

						*sampled = createCameraVertex(scene_camera->pos, importance / pdf);

						// Store the new buffer idx in sampled vertex. It will be used in the next pass to write to the correct pixel.
						int2 imgPos = (int2)((int)(floor(normalizedImagePos.x * image_width + 0.5f)), (int)(floor(normalizedImagePos.y * image_height + 0.5f)));

						imgPos.x = clamp(imgPos.x, 0, image_width - 1);
						imgPos.y = clamp(imgPos.y, 0, image_height - 1);

						sampled->radianceBufferIdx = imgPos.x + imgPos.y * image_width;
						radianceBuffer[curConnectionRayIdx].xyz = lightVertex->throughput * sampled->throughput * evalVertex_f(&scene, lightVertex, sampled, TRANSPORT_MODE_IMPORTANCE); 
						if (isVertexOnSurface(lightVertex))
							radianceBuffer[curConnectionRayIdx] *= absDot(wi, lightInter.sn);
						
						// Set visibility ray						
						float3 rayOrigin = lightInter.p + lightInter.gn * lightVertex->interaction.traceErrorOffset;
						float dist = distance(rayOrigin, scene_camera->pos);
						setRay(connectionRays + curConnectionRayIdx, rayOrigin, dist, (scene_camera->pos - rayOrigin) / dist);
					}
					else
					{
						setRayInactive(connectionRays + curConnectionRayIdx);
					}
				}
				else
				{
					setRayInactive(connectionRays + curConnectionRayIdx);
				}
			}
			else if (s == 1)
			{
				// Connect a sampled light point to camera subpath
				if (isVertexConnectible(cameraVertex))
				{
					float3 wi;
					float pdf;

					int chosenLightIdx = min((int)floor(getSample1D(&sampler) * scene.numLights), scene.numLights - 1);
					float lightPdf = scene.lights[chosenLightIdx].choicePdf;
					RTInteraction camInter = cameraVertex->interaction;
					float3 lightNormal;
					float3 lightPosition;
					float3 Li = sampleLightLi(chosenLightIdx, &scene, &camInter, getSample2D(&sampler), &lightPosition, &lightNormal, &wi, &pdf, connectionRays + curConnectionRayIdx);

					if (isNotNearZero(pdf) && isNotBlack(Li))
					{ 
						// Note: Since s == 1, the min value for t must be 2
						const int sampledLightVertexIdx = bufferIdx * maxDepth + t - 2;
						__global Vertex* sampled = sampledLightVertices + sampledLightVertexIdx;
						float pdfFwd = evalVertexPdfLightOrigin(&scene, sampled, cameraVertex->interaction.p);
						*sampled = createLightVertex(chosenLightIdx, lightPosition, lightNormal, Li / (lightPdf * pdf), pdfFwd, scene.lights[chosenLightIdx].flags);

						float3 f = evaluateMaterial(&scene, cameraVertex->materialIdx, camInter.wo, wi, &camInter, TRANSPORT_MODE_RADIANCE);
						// Shading normal correction isn't necessary here because in the radiance transport mode it is just multiplied by 1.
						//f *= computeShadingNormalCorrection(&cameraVertex->interaction, cameraVertex->interaction.wo, wi, TRANSPORT_MODE_RADIANCE);
						radianceBuffer[curConnectionRayIdx].xyz = cameraVertex->throughput * sampled->throughput * f;
						if (isVertexOnSurface(cameraVertex))
							radianceBuffer[curConnectionRayIdx].xyz *= absDot(wi, camInter.sn);
					}
					else
					{ 
						setRayInactive(connectionRays + curConnectionRayIdx);
					}
				}
			}
			else
			{
				__global Vertex* lightVertex = lightVertices + lightVertexStartIdx + s - 1;
				
				if (isVertexConnectible(cameraVertex) && isVertexConnectible(lightVertex))
				{ 
					float3 lvf = evalVertex_f(&scene, lightVertex, cameraVertex, TRANSPORT_MODE_IMPORTANCE);
					float3 cvf = evalVertex_f(&scene, cameraVertex, lightVertex, TRANSPORT_MODE_RADIANCE);

					float3 lp = lightVertex->interaction.p + lightVertex->interaction.gn * lightVertex->interaction.traceErrorOffset;
					float3 cp = cameraVertex->interaction.p + cameraVertex->interaction.gn * cameraVertex->interaction.traceErrorOffset;
					float3 w = cp - lp;
					float sqDist = dot(w, w);
					float dist = sqrt(sqDist);
					w /= dist;

					if (isNotNearZero(sqDist))
					{ 
						float g = absDot(cameraVertex->interaction.sn, w) * absDot(lightVertex->interaction.sn, w) / sqDist;
						radianceBuffer[curConnectionRayIdx].xyz = lightVertex->throughput * cameraVertex->throughput * lvf * cvf * g;
					}

					if (isNotBlack(radianceBuffer[curConnectionRayIdx].xyz))
					{ 
						setRay(connectionRays + curConnectionRayIdx, lp, dist, w);
					}
					else
					{ 
						setRayInactive(connectionRays + curConnectionRayIdx);
					}
				}
				else
				{ 
					setRayInactive(connectionRays + curConnectionRayIdx);
				}
			}

			++curConnectionRayIdx;
		}
	}

	for (;curConnectionRayIdx < maxConnections; ++curConnectionRayIdx)
	{ 
		setRayInactive(connectionRays + curConnectionRayIdx);
	}
}

// Defined to handle Delta-Functions.
inline float remap0(float f)
{ 
	return f == 0.0f ? 1.0f : f;
}

void atomicAdd_f(volatile __global float *addr, float val)
{
	union {
		unsigned int u32;
		float f32;
	} next, expected, current;

	current.f32 = *addr;
	do
	{
		expected.f32 = current.f32;
		next.f32 = expected.f32 + val;
		current.u32 = atomic_cmpxchg( (volatile __global unsigned int *)addr,
		expected.u32, next.u32);
	} while( current.u32 != expected.u32 );
}

__kernel void ConnectVertices(SCENE_PARAMS,
						 IMAGE_PARAMS,
						 int integrator_frameNum,
						 int maxDepth,
						__global RTBDPTVertex* restrict cameraVertices,
						__global RTBDPTVertex* restrict lightVertices,
						__global RTBDPTVertex* restrict sampledCameraVertices,
						__global RTBDPTVertex* restrict sampledLightVertices,
						__global RTRay* restrict connectionRays,
						__global const int* connectionVisibilities,
						__global const int* restrict cameraVertexCounts,
						__global const int* restrict lightVertexCounts,
						__global float4* tempRadianceBuffer,
						__global float* finalRadianceBuffer)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

	if (gid.x >= image_width || gid.y >= image_height)
		return;

	const int maxLightVertices = maxDepth + 1;
	const int maxCameraVertices = maxDepth + 2;

	const int bufferIdx = gid.x + gid.y * image_width;

	const int cameraVertexCount = cameraVertexCounts[bufferIdx];
	const int lightVertexCount = lightVertexCounts[bufferIdx];

	const int camVertexStartIdx = bufferIdx * maxCameraVertices;
	const int lightVertexStartIdx = bufferIdx * maxLightVertices;

	const int maxConnections = maxCameraVertices * (maxCameraVertices + 1) / 2 - 2;
	int curConnectionRayIdx = bufferIdx * maxConnections;

	MAKE_SCENE(scene);

	// Make sure the order is exactly the same as in PrepareConnections kernel.
	for (int t = 1; t <= cameraVertexCount; ++t)
	{ 
		for (int s = 0; s <= lightVertexCount; ++s)
		{ 
			int curDepth = t + s - 2;
			if ((t == 1 && s == 1) || curDepth < 0 || curDepth > maxDepth)
			{ 
				continue;
			}

#ifdef SHOW_REGULAR_PATH_TRACER_RESULTS
			if (s != 1) continue;
#endif
			__global Vertex* cameraVertex = cameraVertices + camVertexStartIdx + t - 1;

			if (s == 0)
			{ 
				if (isVertexLight(cameraVertex))
				{ 
					// wo is prevVertex->p() - curVertex->p();
					float3 Le = evalLightLe(scene.lights + cameraVertex->lightIdx, cameraVertex->interaction.gn, cameraVertex->interaction.wo);
					tempRadianceBuffer[curConnectionRayIdx].xyz = Le * cameraVertex->throughput;
				}
			}
			else
			{
				// Multiply with visibility term
				float vis = (!isRayActive(connectionRays + curConnectionRayIdx) || connectionVisibilities[curConnectionRayIdx] != -1) ? 0.0f : 1.0f;
				tempRadianceBuffer[curConnectionRayIdx].xyz *= vis; 
			}

			// Compute MIS weight
			float misWeight = 1.0f;

			{
				if (isBlack(tempRadianceBuffer[curConnectionRayIdx].xyz))
					misWeight = 0.0f;
				else if ((s + t) == 2)
					misWeight = 1.0f;
				else
				{ 
					float sumRi = 0.0f;

					__global Vertex* qs = s > 0 ? (lightVertices + lightVertexStartIdx + s - 1) : 0;
					__global Vertex* pt = t > 0 ? (cameraVertices + camVertexStartIdx + t - 1) : 0;
					__global Vertex* qsPrev = s > 1 ? (lightVertices + lightVertexStartIdx + s - 2) : 0;
					__global Vertex* ptPrev = t > 1 ? (cameraVertices + camVertexStartIdx + t - 2) : 0;

					// Vertices need to be temporarily updated for the current t,s combination
					Vertex vBackup;
					if (s == 1)
					{ 
						const int sampledLightVertexIdx = bufferIdx * maxDepth + t - 2;
						__global Vertex* sampled = sampledLightVertices + sampledLightVertexIdx;

						vBackup = *qs;
						*qs = *sampled;
					}
					else if (t == 1)
					{ 
						const int sampledCameraVertexIdx = bufferIdx * maxDepth + s - 2;
						__global Vertex* sampled = sampledCameraVertices + sampledCameraVertexIdx;

						vBackup = *pt;
						*pt = *sampled;
					}

					int qsFlagsBackup;
					int ptFlagsBackup;
					float ptPdfRevBackup;
					float qsPdfRevBackup;
					float ptPrevPdfRevBackup;
					float qsPrevPdfRevBackup;

					if (pt)
					{ 
						ptFlagsBackup = pt->flags;
						// Remove delta flag
						pt->flags &= ~RT_BDPT_VERTEX_FLAG_DELTA;
					}

					if (qs)
					{ 
						qsFlagsBackup = qs->flags;
						qs->flags &= ~RT_BDPT_VERTEX_FLAG_DELTA;
					}

					if (pt)
					{ 
						ptPdfRevBackup = pt->pdfRev;
						pt->pdfRev = s > 0 ? evalVertexPdf(&scene, qs, qsPrev, pt) : evalVertexPdfLightOrigin(&scene, pt, ptPrev->interaction.p);
					}

					if (ptPrev)
					{ 
						ptPrevPdfRevBackup = ptPrev->pdfRev;
						ptPrev->pdfRev = s > 0 ? evalVertexPdf(&scene, pt, qs, ptPrev) : evalVertexPdfLight(&scene, pt, ptPrev);
					}

					if (qs)
					{ 
						qsPdfRevBackup = qs->pdfRev;
						qs->pdfRev = evalVertexPdf(&scene, pt, ptPrev, qs);
					}

					if (qsPrev)
					{ 
						qsPrevPdfRevBackup = qsPrev->pdfRev;
						qsPrev->pdfRev = evalVertexPdf(&scene, qs, pt, qsPrev);
					}

					// Consider connection strategies along camera subpath
					float ri = 1.0f;
					for (int i = t - 1; i > 0; --i)
					{
						int idx = camVertexStartIdx + i;
						ri *= remap0(cameraVertices[idx].pdfRev) / remap0(cameraVertices[idx].pdfFwd);
						if (!isVertexDelta(cameraVertices + idx) && !isVertexDelta(cameraVertices + idx - 1))
							sumRi += ri;
					}

					// Consider connection strategies along light subpath
					ri = 1.0f;
					for (int i = s - 1; i >= 0; --i)
					{
						int idx = lightVertexStartIdx + i;
						ri *= remap0(lightVertices[idx].pdfRev) / remap0(lightVertices[idx].pdfFwd);
						bool isDeltaLightVertex = i > 0 ? (isVertexDelta(lightVertices + idx - 1)) : isVertexDeltaLight(lightVertices + lightVertexStartIdx); 
						if (!isVertexDelta(lightVertices + idx) && !isDeltaLightVertex)
							sumRi += ri;
					}

					// Restore vertex data
					{ 
						if (pt)
						{ 
							pt->flags = ptFlagsBackup;
							pt->pdfRev = ptPdfRevBackup;
						}

						if (qs)
						{ 
							qs->flags = qsFlagsBackup;
							qs->pdfRev = qsPdfRevBackup;
						}

						if (ptPrev)
						{ 
							ptPrev->pdfRev = ptPrevPdfRevBackup;
						}

						if (qsPrev)
						{ 
							qsPrev->pdfRev = qsPrevPdfRevBackup;
						}

						if (s == 1)
						{ 
							*qs = vBackup;
						}
						else if (t == 1)
						{
							*pt = vBackup;
						}
					}

					misWeight = 1.0f / (1.0f + sumRi);
				}
			}

#ifdef SHOW_REGULAR_PATH_TRACER_RESULTS
			if (s == 1)
			{
				const int radianceBufferIdx = bufferIdx * 3;
				
				atomicAdd_f(finalRadianceBuffer + radianceBufferIdx, tempRadianceBuffer[curConnectionRayIdx].x);
				atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 1, tempRadianceBuffer[curConnectionRayIdx].y);
				atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 2, tempRadianceBuffer[curConnectionRayIdx].z);
			}
#else
			//if (t == 1)
			//{
			//	const int sampledCameraVertexIdx = bufferIdx * maxDepth + s - 2;
			//	__global Vertex* sampled = sampledCameraVertices + sampledCameraVertexIdx;
			//
			//	if (isNotBlack(tempRadianceBuffer[curConnectionRayIdx].xyz))
			//	{
			//		const int radianceBufferIdx = sampled->radianceBufferIdx * 3;
			//		atomicAdd_f(finalRadianceBuffer + radianceBufferIdx, tempRadianceBuffer[curConnectionRayIdx].x * misWeight);
			//		atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 1, tempRadianceBuffer[curConnectionRayIdx].y * misWeight);
			//		atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 2, tempRadianceBuffer[curConnectionRayIdx].z * misWeight);
			//	}
			//}
			//else
			//{
			//	const int radianceBufferIdx = bufferIdx * 3;
			//	atomicAdd_f(finalRadianceBuffer + radianceBufferIdx, tempRadianceBuffer[curConnectionRayIdx].x * misWeight);
			//	atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 1, tempRadianceBuffer[curConnectionRayIdx].y * misWeight);
			//	atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 2, tempRadianceBuffer[curConnectionRayIdx].z * misWeight);
			//}

			// Show individual results:
			//if (s == 0 && t == 6)
			{ 
				//misWeight = 1.0f;
				if (t == 1)
				{
					const int sampledCameraVertexIdx = bufferIdx * maxDepth + s - 2;
					__global Vertex* sampled = sampledCameraVertices + sampledCameraVertexIdx;
			
					if (isNotBlack(tempRadianceBuffer[curConnectionRayIdx].xyz))
					{
						const int radianceBufferIdx = sampled->radianceBufferIdx * 3;
						atomicAdd_f(finalRadianceBuffer + radianceBufferIdx, tempRadianceBuffer[curConnectionRayIdx].x * misWeight);
						atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 1, tempRadianceBuffer[curConnectionRayIdx].y * misWeight);
						atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 2, tempRadianceBuffer[curConnectionRayIdx].z * misWeight);
					}
				}
				else
				{
					const int radianceBufferIdx = bufferIdx * 3;
					atomicAdd_f(finalRadianceBuffer + radianceBufferIdx, tempRadianceBuffer[curConnectionRayIdx].x * misWeight);
					atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 1, tempRadianceBuffer[curConnectionRayIdx].y * misWeight);
					atomicAdd_f(finalRadianceBuffer + radianceBufferIdx + 2, tempRadianceBuffer[curConnectionRayIdx].z * misWeight);
				}
			}
#endif
			
			++curConnectionRayIdx;
		}
	}
}

// Copy source buffer defined by float* to a destination buffer defined by float4*.
__kernel void CopyBuffer(
				int width,
                int height,
				__global const float* srcBuffer,
				__global float4* destBuffer)
{ 
    int2 gid = (int2)(get_global_id(0), get_global_id(1));

    // Check borders
    if (gid.x < width && gid.y < height)
    {
        int bufferIdx = gid.y * width + gid.x;
		const int i = bufferIdx * 3;
		float4 val = (float4)(srcBuffer[i], srcBuffer[i + 1], srcBuffer[i + 2], 0.0f);
		destBuffer[bufferIdx] = val;
    }
}

#endif