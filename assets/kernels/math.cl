#ifndef MATH_CL
#define MATH_CL

/**
* Left-handed coordinate system: x: right, y: up, z: inside the screen
*/

#define PI 3.14159265359f
#define PI_INV 0.31830988618f
#define PI2_INV 0.15915494309f
#define PI4_INV 0.07957747154f
#define PI2 6.28318530718f
#define PI_DIV_4 0.78539816339f
#define PI_DIV_2 1.57079632679f

#define EPSILON 1e-8f

inline float3 makeSphericalDirection(float sinTheta, float cosTheta, float phi)
{ 
	return (float3)(sinTheta * cos(phi),
					cosTheta,
					sinTheta * sin(phi));
}

inline float3 makeSphericalDirectionFromBasis(float sinTheta, float cosTheta, float phi, float3 x, float3 y, float3 z)
{ 
	return sinTheta * cos(phi) * x + cosTheta * y + sinTheta * sin(phi) * z;
}

// v is a unit vector
inline float sphericalTheta(float3 v)
{ 
	return acos(clamp(v.y, -1.0f, 1.0f));
}

// v is a unit vector
inline float sphericalPhi(float3 v)
{ 
	float p = atan2(v.z, v.x);
	return (p < 0) ? (p + 2 * PI) : p;
}

inline float degToRad(float deg)
{ 
	return deg * PI / 180.0f;
}

inline float radToDeg(float rad)
{ 
	return rad * 180.0f * PI_INV;
}

float3 computeOrthogonalVector(float3 n)
{
	// Cross product with (0,1,0)
	if (fabs(n.z) > 0.0f)
	{ 
		float d = sqrt(n.z * n.z + n.x * n.x);
		return (float3)(-n.z / d, 0.0f, n.x / d);
	}
	else // Cross product with (0,0,1)
	{
		float d = sqrt(n.y * n.y + n.x * n.x);
		return (float3)(n.y / d, -n.x / d, 0.0f);
	}
}

inline float absDot(float3 v0, float3 v1)
{ 
	return fabs(dot(v0, v1));
}

inline bool isNearZero(float v)
{ 
	return fabs(v) < 1e-8f;
}

inline bool isNotNearZero(float v)
{ 
	return fabs(v) > 1e-8f;
}

inline float distanceSquared(float3 p0, float3 p1)
{ 
	return dot(p0-p1, p0-p1);
}

inline float3 lerpDirection(float3 d0, float3 d1, float3 d2, float3 d3, float t0, float t1)
{
    return normalize(mix(mix(d0, d1, t0), mix(d3, d2, t0), t1));
}

#endif