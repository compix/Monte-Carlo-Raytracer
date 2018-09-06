#ifndef TONE_MAPPING_CL
#define TONE_MAPPING_CL
#include "image_samplers.cl"
#include "colors.cl"

__kernel void ComputeLogAveragePass1(read_only image2d_t originalImage, __global float4* restrict intermediateBuffer)
{
	int width = get_image_width(originalImage);
	float4 sum = (float4)(0.0f);
	float4 delta = (float4)(0.00000001f);
	int y = get_global_id(0);

	for (int x = 0; x < width; ++x)
	{
		float4 Lw = read_imagef(originalImage, NON_NORMALIZED_NEAREST_CLAMP_TO_EDGE_SAMPLER, (int2)(x, y));
		sum += log(delta + Lw);
	}

	intermediateBuffer[y] = sum;
}

__kernel void ComputeLogAveragePass2(__global float4* restrict intermediateBuffer, int numberOfValues, int N)
{ 
	float4 sum = (float4)(0.0f);

	for (int i = 0; i < numberOfValues; ++i)
		sum += intermediateBuffer[i];

	intermediateBuffer[numberOfValues] = exp(sum) / N;
}

float toneMapSimple(float L)
{ 
	return L / (1.0f + L);
}

float toneMapControlled(float L, float Lwhite)
{ 
	return L * (1.0f + L / (Lwhite * Lwhite)) / (1.0f + L);
}

__kernel void ReinhardToneMapping(
						  int width,
						  int height,
						  float Lwhite,
						  read_only image2d_t originalImage,
						  write_only image2d_t outputImage)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

	if (gid.x >= width || gid.y >= height)
		return;

	float4 rgba = read_imagef(originalImage, NON_NORMALIZED_NEAREST_CLAMP_TO_EDGE_SAMPLER, gid);

	// Compute luminance (this is the computation of the y component of xyz colors)
	float L = computeLuminanceFromRGB(rgba.xyz); 0.2126f * rgba.x + 0.7152f * rgba.y + 0.0722f * rgba.z;
	float tL = toneMapControlled(L, Lwhite);

	// Convert back to rgb
	rgba.xyz *= tL / L;
	write_imagef(outputImage, gid, rgba);
}

#endif // TONE_MAPPING_CL