#ifndef COLORS_CL
#define COLORS_CL

#define RGB_BLACK (float3)(0.0f, 0.0f, 0.0f);
#define RGB_WHITE (float3)(1.0f, 1.0f, 1.0f);
#define RGB_RED (float3)(1.0f, 0.0f, 0.0f);
#define RGB_GREEN (float3)(0.0f, 1.0f, 0.0f);
#define RGB_BLUE (float3)(0.0f, 0.0f, 1.0f);

#define RGBA_BLACK (float4)(0.0f, 0.0f, 0.0f, 0.0f);
#define RGBA_WHITE (float4)(1.0f, 1.0f, 1.0f, 0.0f);
#define RGBA_RED (float4)(1.0f, 0.0f, 0.0f, 0.0f);
#define RGBA_GREEN (float4)(0.0f, 1.0f, 0.0f, 0.0f);
#define RGBA_BLUE (float4)(0.0f, 0.0f, 1.0f, 0.0f);

/**
* Computes the Y component of the XYZ color space.
*/
inline float computeLuminanceFromRGB(float3 rgb)
{ 
	return 0.212671f * rgb.x + 0.715160f * rgb.y + 0.072169f * rgb.z;
}

inline float3 rgbToXYZ(float3 rgb)
{ 
	return (float3)(0.412453f * rgb.x + 0.357580f * rgb.y + 0.180423f * rgb.z,
					0.212671f * rgb.x + 0.715160f * rgb.y + 0.072169f * rgb.z,
					0.019334f * rgb.x + 0.119193f * rgb.y + 0.950227f * rgb.z);
}

inline float3 xyzToRGB(float3 xyz)
{
	return (float3)( 3.240479f * xyz.x - 1.537150f * xyz.y - 0.498535f * xyz.z,
					-0.969256f * xyz.x + 1.875991f * xyz.y + 0.041556f * xyz.z,
					 0.055648f * xyz.x - 0.204043f * xyz.y + 1.057311f * xyz.z);
}

#endif