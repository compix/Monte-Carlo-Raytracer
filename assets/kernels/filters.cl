#ifndef FILTERS_CL
#define FILTERS_CL

/**
* Implementations based on "Physically Based Rendering From Theory to Implementation Third Edition".
*
* Note: Filter inputs must be within the extent (radius * 2) of the filters.
*/

#include "math.cl"

inline float evaluateBoxFilter(float2 p, float2 radius)
{ 
	return 1.0f;
}

inline float evaluateTriangleFilter(float2 p, float2 radius)
{ 
	return max(0.0f, radius.x - fabs(p.x)) * max(0.0f, radius.y - fabs(p.y));
}

inline float gaussian(float d, float alpha, float expv)
{ 
	return max(0.0f, exp(-alpha * d * d) - expv);
}

inline float evaluateGaussianFilter(float2 p, float alpha, float expX, float expY)
{ 
	return gaussian(p.x, alpha, expX) * gaussian(p.y, alpha, expY);
}

inline float mitchell1D(float x, float B, float C)
{ 
	x = fabs(2.0f * x);
	if (x > 1.0f)
		return ((-B - 6*C) * x*x*x + (6*B + 30*C) * x*x + (-12*B - 48*C) * x + (8*B + 24*C)) * (1.f/6.f);
	else
		return ((12 - 9*B - 6*C) * x*x*x + (-18 + 12*B + 6*C) * x*x + (6 - 2*B)) * (1.f/6.f);
}

// Note: Mitchell and Netravali recommend using values for B and C such that B + 2C = 1.
inline float evaluateMitchellFilter(float2 p, float2 radius, float B, float C)
{ 
	return mitchell1D(p.x / radius.x, B, C) * mitchell1D(p.y / radius.y, B, C);
}

inline float sinc(float x)
{ 
	x = fabs(x);
	if (x < 1e-5)
		return 1.0f;

	return sin(PI * x) / (PI * x);
}

inline float windowedSinc(float x, float radius, float tau)
{ 
	x = fabs(x);
	if (x > radius)
		return 0.0f;

	float lanczos = sinc(x / tau);
	return sinc(x) * lanczos;
}

inline float evaluateLanczosFilter(float2 p, float2 radius, float tau)
{ 
	return windowedSinc(p.x, radius.x, tau) * windowedSinc(p.y, radius.y, tau);
}

#endif // FILTERS_CL