#ifndef RECONSTRUCTION_CL
#define RECONSTRUCTION_CL
#include "kernel_data.h"
#include "filters.cl"

__kernel void ReconstructionPass(
				int width,
                int height,
                int integrator_frameNum,
				__global const RTFilterProperties* filterProperties,
				__global float4* radianceBuffer,
				__global float4* weightedRadianceBuffer,
				__global float* filterWeightsBuffer,
				write_only image2d_t image)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

    if (gid.x < width && gid.y < height)
    {
        int bufferIdx = gid.y * width + gid.x;
		float4 radiance = clamp(radianceBuffer[bufferIdx], 0.0f, RT_MAX_ALLOWED_RADIANCE);

		float filterWeight;

		switch (filterProperties->filterType)
		{ 
		case RT_BOX_FILTER:
			filterWeight = 1.0f;
			break;
		case RT_TRIANGLE_FILTER:
			filterWeight = evaluateTriangleFilter(filterProperties->pixelOffset, filterProperties->radius);
			break;
		case RT_GAUSSIAN_FILTER:
			filterWeight = evaluateGaussianFilter(filterProperties->pixelOffset, filterProperties->gaussianAlpha, filterProperties->gaussianExpX, filterProperties->gaussianExpY);
			break;
		case RT_MITCHELL_FILTER:
			filterWeight = evaluateMitchellFilter(filterProperties->pixelOffset, filterProperties->radius, 
												  filterProperties->mitchellB, filterProperties->mitchellC);
			break;
		case RT_LANCZOS_SINC_FILTER:
			filterWeight = evaluateLanczosFilter(filterProperties->pixelOffset, filterProperties->radius, filterProperties->lanczosSincTau);
			break;
		}

		if (integrator_frameNum == 0)
		{ 
			weightedRadianceBuffer[bufferIdx] = radiance * filterWeight;
			filterWeightsBuffer[bufferIdx] = filterWeight;
		}
		else
		{ 
			weightedRadianceBuffer[bufferIdx] += radiance * filterWeight;
			filterWeightsBuffer[bufferIdx] += filterWeight;
		}

		// Compute the final result to be displayed on the screen.
		float4 finalResult = weightedRadianceBuffer[bufferIdx] / filterWeightsBuffer[bufferIdx];
        write_imagef(image, gid, finalResult);
    }
}

// Computes reconstruction for all filters (box, triangle, gaussian, mitchell, lanczos).
// Useful to compare filters.
__kernel void ReconstructionPassAllFilters(
				int width,
                int height,
                int integrator_frameNum,
				__global const RTFilterProperties* filterProperties,
				__global float4* radianceBuffer,
				__global float4* weightedRadianceBuffer,
				__global float* filterWeightsBuffer)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

    if (gid.x < width && gid.y < height)
    {
        int bufferIdx = gid.y * width + gid.x;

		float boxFilterWeight = 1.0f;
		float triangleFilterWeight = evaluateTriangleFilter(filterProperties->pixelOffset, filterProperties->radius);
		float gaussianFilterWeight = evaluateGaussianFilter(filterProperties->pixelOffset, filterProperties->gaussianAlpha, filterProperties->gaussianExpX, filterProperties->gaussianExpY);
		float mitchellFilterWeight = evaluateMitchellFilter(filterProperties->pixelOffset, filterProperties->radius, filterProperties->mitchellB, filterProperties->mitchellC);
		float lanczosFilterWeight = evaluateLanczosFilter(filterProperties->pixelOffset, filterProperties->radius, filterProperties->lanczosSincTau);

		float4 radiance = clamp(radianceBuffer[bufferIdx], 0.0f, RT_MAX_ALLOWED_RADIANCE);

		if (integrator_frameNum == 0)
		{ 
			weightedRadianceBuffer[bufferIdx + width * height * RT_BOX_FILTER] = radiance * boxFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_BOX_FILTER] = boxFilterWeight;

			weightedRadianceBuffer[bufferIdx + width * height * RT_TRIANGLE_FILTER] = radiance* triangleFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_TRIANGLE_FILTER] = triangleFilterWeight;

			weightedRadianceBuffer[bufferIdx + width * height * RT_GAUSSIAN_FILTER] = radiance * gaussianFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_GAUSSIAN_FILTER] = gaussianFilterWeight;

			weightedRadianceBuffer[bufferIdx + width * height * RT_MITCHELL_FILTER] = radiance * mitchellFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_MITCHELL_FILTER] = mitchellFilterWeight;

			weightedRadianceBuffer[bufferIdx + width * height * RT_LANCZOS_SINC_FILTER] = radiance * lanczosFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_LANCZOS_SINC_FILTER] = lanczosFilterWeight;
		}
		else
		{ 
			weightedRadianceBuffer[bufferIdx + width * height * RT_BOX_FILTER] += radiance * boxFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_BOX_FILTER] += boxFilterWeight;

			weightedRadianceBuffer[bufferIdx + width * height * RT_TRIANGLE_FILTER] += radiance * triangleFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_TRIANGLE_FILTER] += triangleFilterWeight;

			weightedRadianceBuffer[bufferIdx + width * height * RT_GAUSSIAN_FILTER] += radiance * gaussianFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_GAUSSIAN_FILTER] += gaussianFilterWeight;

			weightedRadianceBuffer[bufferIdx + width * height * RT_MITCHELL_FILTER] += radiance * mitchellFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_MITCHELL_FILTER] += mitchellFilterWeight;

			weightedRadianceBuffer[bufferIdx + width * height * RT_LANCZOS_SINC_FILTER] += radiance * lanczosFilterWeight;
			filterWeightsBuffer[bufferIdx + width * height * RT_LANCZOS_SINC_FILTER] += lanczosFilterWeight;
		}
    }
}

__kernel void CopyReconstructionResult(
				int width,
                int height,
				int curFilterIdx,
				__global float4* weightedRadianceBuffer,
				__global float* filterWeightsBuffer,
				write_only image2d_t image)
{
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

    if (gid.x < width && gid.y < height)
    {
        int bufferIdx = gid.y * width + gid.x;

		// Compute the final result to be displayed on the screen.
		float4 finalResult = weightedRadianceBuffer[bufferIdx + width * height * curFilterIdx] / filterWeightsBuffer[bufferIdx + width * height * curFilterIdx];
        write_imagef(image, gid, finalResult);
    }
}

#endif // RECONSTRUCTION_CL