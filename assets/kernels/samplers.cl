#ifndef SAMPLERS_CL
#define SAMPLERS_CL

/** Implementation of Sobol based on "Physically Based Rendering From Theory to Implementation Third Edition"
*
* Note: 1/2^32 = 0x1p-32f = (1.f / (1UL << 32)) = 1.f / 0x1p32f
*/

#include <math.cl>
#include <kernel_data.h>
#include <rng.cl>

#define NUM_SOBOL_DIMENSIONS 1024
#define SOBOL_MATRIX_SIZE 52

#define RT_SAMPLER_SOBOL 0
#define RT_SAMPLER_RANDOM 1
#define RT_SAMPLER RT_SAMPLER_RANDOM 

typedef struct
{ 
	uint idx;
	uint dimension; 
	uint scramble; 
	__global const uint* sobolMatrices;
} Sampler;

inline uint countTrailingZeros(uint v)
{ 
	return 32 - clz(v & -v);
}

inline uint multiplyGenerator(__global const uint* C, uint a)
{ 
	uint v = 0;
	for (int i = 0; a != 0; ++i, a >>=1)
		if (a & 1)
			v ^= C[i];

	return v;
}

inline float sampleGeneratorMatrix(__global const uint* C, uint a, uint scramble)
{ 
	return (multiplyGenerator(C, a) ^ scramble) * 0x1p-32f;
}

inline uint grayCode(uint n)
{ 
	return (n >> 1) ^ n;
}

// Fills p with samples
inline void grayCodeSample(__global const uint* C, uint n, uint scramble, float* p)
{ 
	uint v = scramble;
	for (uint i = 0; i < n; ++i)
	{ 
		p[i] = v * 0x1p-32f;
		v ^= C[countTrailingZeros(i+1)];
	}
}

inline float sobolSample(uint idx, uint dimension, uint scramble, __global const uint* sobolMatrices)
{ 
	uint v = scramble;
	for (uint i = dimension * SOBOL_MATRIX_SIZE; idx != 0; idx >>= 1, ++i)
		if (idx & 1)
			v ^= sobolMatrices[i];

	return v * 0x1p-32f;
}

#if RT_SAMPLER == RT_SAMPLER_SOBOL
	#define MAKE_SAMPLER(sampler, bufferIdx, offset) Sampler sampler;\
								  sampler.idx = bufferIdx + integrator_frameNum * image_width * image_height;\
								  sampler.dimension = 0;\
								  uint sampler_seed = randSequenceSeed((integrator_frameNum + 1) * (offset + 1));\
								  sampler.scramble = randUInt(&sampler_seed);\
								  sampler.sobolMatrices = scene_sobolMatrices;

#elif RT_SAMPLER == RT_SAMPLER_RANDOM
	#define MAKE_SAMPLER(sampler, bufferIdx, offset) Sampler sampler;\
								  sampler.idx = randSequenceSeed(bufferIdx + (integrator_frameNum + 1) * image_width * image_height * (offset + 1));
#endif
							
inline void initSampler(Sampler* sampler, uint idx, uint dimension, uint scramble, __global const uint* sobolMatrices)
{ 
#if RT_SAMPLER == RT_SAMPLER_SOBOL
	sampler->idx = idx;
	sampler->dimension = dimension;
	sampler->scramble = scramble;
	sampler->sobolMatrices = sobolMatrices;
#elif RT_SAMPLER == RT_SAMPLER_RANDOM
	sampler->idx = idx;
#endif
}

inline float getSample1D(Sampler* sampler)
{ 
#if RT_SAMPLER == RT_SAMPLER_SOBOL
	float u = sobolSample(sampler->idx, sampler->dimension, sampler->scramble, sampler->sobolMatrices);
	sampler->dimension++;
	return u;
#elif RT_SAMPLER == RT_SAMPLER_RANDOM
	return randFloat(&sampler->idx);
#endif
}

inline float2 getSample2D(Sampler* sampler)
{ 
#if RT_SAMPLER == RT_SAMPLER_SOBOL
	float2 u;
	u.x = sobolSample(sampler->idx, sampler->dimension, sampler->scramble, sampler->sobolMatrices);
	sampler->dimension++;
	u.y = sobolSample(sampler->idx, sampler->dimension, sampler->scramble, sampler->sobolMatrices);
	sampler->dimension++;
	return u;
#elif RT_SAMPLER == RT_SAMPLER_RANDOM
	return (float2)(randFloat(&sampler->idx), randFloat(&sampler->idx));
#endif
}

/**
* @param u In [0,1]^2
*/
float3 uniformSampleHemisphere(float2 u)
{ 
	float y = u.x;
	float r = sqrt(max(0.0f, 1.0f - y * y));
	float phi = PI2 * u.y;
	return (float3)(r * cos(phi), y, r * sin(phi));
}

float uniformHemispherePdf()
{ 
	return PI2_INV;
}

/**
* @param u In [0,1]^2
*/
float3 uniformSampleSphere(float2 u)
{ 
	float y = 1.0f - 2.0f * u.x;
	float r = sqrt(fmax(0.0f, 1.0f - y * y));
	float phi = 2.0f * PI * u.y;
	return (float3)(r * cos(phi), y, r * sin(phi));
}

float uniformSpherePdf()
{ 
	return PI4_INV;
}

/**
* @param u In [0,1]^2
*/
float2 uniformSampleDisk(float2 u)
{ 
	float r = sqrt(u.x);
	float theta = PI2 * u.y;
	return (float2)(r * cos(theta), r * sin(theta));
}

/**
* @param u In [0,1]^2
*/
float2 concentricSampleDisc(float2 u)
{ 
	// Map to [-1,1]^2
	float2 uOffset = 2.0f * u - (float2)(1.0f);

	if (u.x < 1e-8f && u.y < 1e-8f)
		return (float2)(0.0f);

	float theta, r;

	if (fabs(uOffset.x) > fabs(uOffset.y))
	{ 
		r = uOffset.x;
		theta = PI_DIV_4 * (uOffset.y / uOffset.x);
	}
	else
	{ 
		r = uOffset.y;
		theta = PI_DIV_2 - PI_DIV_4 * (uOffset.x / uOffset.y);
	}

	return r * (float2)(cos(theta), sin(theta));
}

float3 cosineSampleHemisphere(float2 u)
{ 
	float2 d = concentricSampleDisc(u);
	float y = sqrt(fmax(0.0f, 1.0f - d.x * d.x - d.y * d.y));
	return (float3)(d.x, y, d.y);
}

inline float cosineHemispherePdf(float cosTheta)
{ 
	return cosTheta * PI_INV;
}

float uniformConePdf(float cosThetaMax)
{ 
	return 1.0f / (2.0f * PI * (1.0f - cosThetaMax));
}

/**
* @param u In [0,1]^2
*/
float3 uniformSampleCone(float2 u, float cosThetaMax)
{ 
	float cosTheta = (1.0f - u.x) + u.x * cosThetaMax;
	float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	float phi = u.y * 2.0f * PI;
	return (float3)(cos(phi) * sinTheta, cosTheta, sin(phi) * sinTheta);
}

/**
* Note: The pdf is 1 / area(triangle)
*
* @param u In [0,1]^2
* @return barycentrics
*/
float2 uniformSampleTriangle(float2 u)
{ 
	float su0 = sqrt(u.x);
	return (float2)(1.0f - su0, u.y * su0);
}

inline float evalBalanceHeuristic(int nf, float fPdf, int ng, float gPdf)
{ 
	return (nf * fPdf) / (nf * fPdf + ng * gPdf);
}

/**
* Power heurstic with beta = 2 as determined by Veach to be a good choice.
*/
inline float evalPowerHeuristic(int nf, float fPdf, int ng, float gPdf)
{ 
	float f = nf * fPdf;
	float g = ng * gPdf;
	return (f * f) / (f * f + g * g);
}

float computeDiskArea(float radius)
{ 
	return PI * radius * radius;
}

/**
* @param p World position of the disk.
* @param n World normal of the disk.
* @param radius Radius of the disk.
* @param u Sample in [0,1]^2
*/
RTInteraction sampleDisk(float3 p, float3 n, float radius, float2 u, float* pdf)
{ 
    float2 p2d = concentricSampleDisc(u);
    RTInteraction it;
    it.gn = n;
	float3 t = computeOrthogonalVector(n);
	float3 b = normalize(cross(n, t));
    it.p = p + t * p2d.x * radius + b * p2d.y * radius;
    *pdf = 1.0f / computeDiskArea(radius);
    return it;
}

/**
* p0,p1,p2 are the positions of the triangle in world space.
* @param u Sample in [0,1]^2
*/
RTInteraction sampleTriangle(float3 p0, float3 p1, float3 p2, float2 u, float* pdf)
{ 
    float2 b = uniformSampleTriangle(u);
    RTInteraction it;
    it.p = b.x * p0 + b.y * p1 + (1 - b.x - b.y) * p2;
	float3 c = cross(p1 - p0, p2 - p0);
    it.gn = normalize(c);
	float area = length(c) * 0.5f;
    *pdf = 1.0f / area;
    return it;
}

#endif