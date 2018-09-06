#ifndef DENOISE_CL
#define DENOISE_CL
#include "image_samplers.cl"

// Based on https://en.wikipedia.org/wiki/Bilateral_filter
__kernel void BilateralDenoise(
					int width,
					int height,
					int filterRadius,
					float sigmaSpatial,
					float sigmaRange,
					read_only image2d_t originalImage,
					write_only image2d_t denoisedImage)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

	if (gid.x >= width || gid.y >= height || filterRadius <= 0 || sigmaSpatial <= 0.0f || sigmaRange <= 0.0f)
		return;

	float sdSq = sigmaSpatial * sigmaSpatial;
	float srSq = sigmaRange * sigmaRange;
	float4 finalColor = (float4)(0.0f);
	float4 originalColor = read_imagef(originalImage, NON_NORMALIZED_NEAREST_CLAMP_TO_EDGE_SAMPLER, gid);

	float weightSum = 0.0f;
	for (int rx = -filterRadius; rx <= filterRadius; ++rx)
	{
		int x = clamp(rx + gid.x, 0, width - 1);

		for (int ry = -filterRadius; ry <= filterRadius; ++ry)
		{ 
			int y = clamp(ry + gid.y, 0, height - 1);

			float4 kernelColor = read_imagef(originalImage, NON_NORMALIZED_NEAREST_CLAMP_TO_EDGE_SAMPLER, (int2)(x, y));

			float4 colorDiff = originalColor - kernelColor;

			float w = exp(-((gid.x - x) * (gid.x - x) + (gid.y - y) * (gid.y - y)) / (2.0f * sdSq) - dot(colorDiff, colorDiff) / (2.0f * srSq));

			weightSum += w;
			finalColor += w * kernelColor;
		}
	}

	finalColor /= weightSum;
	write_imagef(denoisedImage, gid, finalColor);
}

#endif