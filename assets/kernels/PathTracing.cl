#ifndef PATH_TRACING_CL
#define PATH_TRACING_CL

#include "rng.cl"
#include "kernel_data.h"
#include "samplers.cl"
#include "bxdfs.cl"
#include "materials.cl"
#include "geometry.cl"
#include "lights.cl"
#include "image_samplers.cl"

__kernel void GeneratePerspectiveRays(__global RTRay* trace_rays, 
									  __global RTRayDifferentials* rayDifferentials,
									  __global const RTPinholeCamera* cam)
{
    int2 gid = (int2)(get_global_id(0), get_global_id(1));

    // Check borders
    if (gid.x < cam->width && gid.y < cam->height)
    {
		float2 r = (float2)(1.0f / cam->width, 1.0f / cam->height);

		const float2 uv = (float2)(gid.x * r.x, gid.y * r.y);
        // Perspective view
        int bufferIdx = gid.y * cam->width + gid.x;
		setRay(trace_rays + bufferIdx, cam->pos, 1000.0f, lerpDirection(cam->r00, cam->r10, cam->r11, cam->r01, uv.x, uv.y));

		// Compute ray differentials
		rayDifferentials[bufferIdx].xOrigin = cam->pos;
		rayDifferentials[bufferIdx].yOrigin = cam->pos;
		rayDifferentials[bufferIdx].xDirection = lerpDirection(cam->r00, cam->r10, cam->r11, cam->r01, uv.x + r.x, uv.y);
		rayDifferentials[bufferIdx].yDirection = lerpDirection(cam->r00, cam->r10, cam->r11, cam->r01, uv.x, uv.y + r.y);
    }
}

__kernel void ClearImage(
                int width,
                int height,
                write_only image2d_t image)
{
    int2 globalid;
    globalid.x  = get_global_id(0);
    globalid.y  = get_global_id(1);
    if (globalid.x < width && globalid.y < height)
    {
        write_imagef(image, (int2)(globalid.x, globalid.y), (float4)(0.0f, 0.0f, 0.0f, 0.0f));
    }
}

// Note: PARAMS defines are in kernel_data.h
__kernel void PathTracing(
				SCENE_PARAMS,
				IMAGE_PARAMS,
				TRACE_PARAMS,
				INTEGRATOR_PARAMS)
{
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

    // Check borders
    if (gid.x >= image_width || gid.y >= image_height) return;

	MAKE_SCENE(scene);

    int bufferIdx = gid.y * image_width + gid.x;

	RTIntersection isect = trace_isects[bufferIdx];
    int shapeIdx = isect.shapeid;
    int primitiveIdx = isect.primid;
    float4 radiance = (float4)(0.f, 0.f, 0.f, 0.f);

    if (isRayActive(trace_rays + bufferIdx) && shapeIdx != -1 && primitiveIdx != -1 && scene.numLights > 0)
    {
        // Calculate lighting
		RTInteraction si = computeSurfaceInteraction(&scene, &isect);
		si.wo = -trace_rays[bufferIdx].d.xyz;
		const bool isBackfacing = dot(si.gn, si.wo) < 0.0f;
		si.traceErrorOffset = isBackfacing ? -RT_TRACE_OFFSET : RT_TRACE_OFFSET;
		applyNormalMapping(&scene, scene_shapes[shapeIdx].materialId, &si);

		if (integrator_bounceIdx == 0)
		{ 
			integrator_throughputBuffer[bufferIdx].throughput = (float3)(1.0f);
		}

		// Add emitted light at intersection if applicable
		// If a ray hits an emissive objects then the contribution must be accounted for if
		//	a. It is the first hit from the camera.
		//	b. The last bounce was specular.
		// Otherwise emission is ignored because it has been already handled in the light sampling computation at the previous vertex.
		const bool isEmitter = scene_shapes[shapeIdx].lightID != RT_INVALID_ID;
		const bool sampledSpecular = (BSDF_SPECULAR & integrator_throughputBuffer[bufferIdx].prevBsdfFlags) == BSDF_SPECULAR;
		if (isEmitter && (integrator_bounceIdx == 0 || sampledSpecular))
		{ 
			int lightID = scene_shapes[shapeIdx].lightID;
			float3 Le = evalLightLe(scene.lights + lightID, si.gn, si.wo);
			radiance.xyz += integrator_throughputBuffer[bufferIdx].throughput * Le;
			setRayInactive(trace_shadowRays + bufferIdx);
			setRayInactive(trace_rays + bufferIdx);
			integrator_throughputBuffer[bufferIdx].ignoreOcclusion = 1;
		}
		else
		{ 
			MAKE_SAMPLER(sampler, bufferIdx, integrator_bounceIdx);
			integrator_throughputBuffer[bufferIdx].ignoreOcclusion = 0;

			// Compute estimate of direct lighting
			{
				// Sample one light source
				float lightPdf;
				uint lightIdx = ((uint)floor(getSample1D(&sampler) * scene.numLights));
				lightIdx %= scene.numLights;
				float3 wi;
				float2 u = getSample2D(&sampler);

				float3 unusedLightNormal;
				float3 unusedLightPosition;
				float3 Li = sampleLightLi(lightIdx, &scene, &si, u, &unusedLightPosition, &unusedLightNormal, &wi, &lightPdf, trace_shadowRays + bufferIdx);
				lightPdf *= scene.lights[lightIdx].choicePdf;
				float3 L = (float3)(0.0f);

				int materialId = scene_shapes[shapeIdx].materialId;
				if (materialId != RT_INVALID_ID)
				{
					float3 bsdf = evaluateMaterial(&scene, materialId, si.wo, wi, &si, TRANSPORT_MODE_RADIANCE);
					bsdf *= absDot(wi, si.sn);

					if (!isNearZero(lightPdf))
					{ 
						L = Li * bsdf / lightPdf;
					}
				}

				radiance.xyz += integrator_throughputBuffer[bufferIdx].throughput * L;

			} // End of Compute estimate of direct lighting

			// Sample bsdf to extend path
			if (integrator_bounceIdx + 1 < integrator_maxDepth)
			{
				float2 bsdfSample = getSample2D(&sampler);
				float pdf;

				if (scene_shapes[shapeIdx].materialId != RT_INVALID_ID)
				{
					BxDFType sampledType;
					int unused;
					float3 wi;
					float3 bsdfBounce = sampleMaterial(&scene, scene_shapes[shapeIdx].materialId, &si, bsdfSample, TRANSPORT_MODE_RADIANCE, BSDF_ALL, si.wo, &wi, &pdf, &unused, &sampledType);

					integrator_throughputBuffer[bufferIdx].prevBsdfFlags = sampledType;

					// If pdf is near 0 or the color is black then the ray can be terminated
					if (isNearZero(pdf) || isBlack(bsdfBounce))
					{ 
						setRayInactive(trace_rays + bufferIdx);
					}
					else
					{ 
						bsdfBounce /= pdf;
						float3 throughput = bsdfBounce * absDot(wi, si.sn);
						integrator_throughputBuffer[bufferIdx].throughput *= throughput;

						// Set ray for next bounce
						float traceErrorOffset = si.traceErrorOffset;
						if ((sampledType & BSDF_TRANSMISSION) != 0 && dot(si.gn, wi) * sign(traceErrorOffset) < 0.0f)
							traceErrorOffset *= -1.0f;


						setRay(trace_rays + bufferIdx, si.p + si.gn * traceErrorOffset, RT_MAX_TRACE_DISTANCE, wi);
					}
				}
				else
					setRayInactive(trace_rays + bufferIdx);
			}
		}
    }
	else
	{
		setRayInactive(trace_rays + bufferIdx);
	}

	integrator_radianceBuffer[bufferIdx] = radiance;
}

__kernel void ShadowPass(
				__global const RTRay* trace_rays,
                __global const int* occlusion,
                int width,
                int height,
				int integrator_bounceIdx,
				__global RTThroughput* integrator_throughputBuffer,
				__global float4* tempRadianceBuffer,
				__global float4* integrator_radianceBuffer)
{
    int2 gid = (int2)(get_global_id(0), get_global_id(1));

    // Check borders
    if (gid.x >= width && gid.y >= height)
		return;

    int bufferIdx = gid.y * width + gid.x;
    float4 radiance = tempRadianceBuffer[bufferIdx];

#ifdef RT_ENABLE_SHADOWS
	if (integrator_throughputBuffer[bufferIdx].ignoreOcclusion == 0)
	{
		float V = (!isRayActive(trace_rays + bufferIdx) || occlusion[bufferIdx] != -1) ? 0.0f : 1.0f;
		radiance *= V;
	}
#endif

	if (integrator_bounceIdx == 0)
		integrator_radianceBuffer[bufferIdx] = radiance;
	else
		integrator_radianceBuffer[bufferIdx] += radiance;
}

#endif // PATH_TRACING_CL