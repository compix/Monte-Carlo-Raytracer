#ifndef MATRIX_CL
#define MATRIX_CL

// Matrix in row major order
typedef struct
{
	float4 m0;
	float4 m1;
	float4 m2;
	float4 m3;
} mat4;

// Matrix in row major order
typedef struct
{ 
	float2 m0;
	float2 m1;
} mat2;

inline mat2 makeMat2(float m00, float m01, 
				     float m10, float m11)
{ 
	mat2 m;
	m.m0.x = m00;
	m.m0.y = m01;
	m.m1.x = m10;
	m.m1.y = m11;
	return m;
}

inline mat2 makeMat2FromRows(float2 r0, float r1)
{ 
	mat2 m;
	m.m0 = r0;
	m.m1 = r1;
	return m;
}

inline float computeMat2Det(mat2 m)
{ 
	return m.m0.x * m.m1.y - m.m1.x * m.m0.y;
}

float3 transformVector3(mat4 m, float3 v)
{
    float3 result;
    result.x = dot(m.m0.xyz, v);
    result.y = dot(m.m1.xyz, v);
    result.z = dot(m.m2.xyz, v);
    return result;
}

float3 transformPoint3(mat4 m, float3 v)
{
    float3 result;
    result.x = dot(m.m0.xyz, v) + m.m0.w;
    result.y = dot(m.m1.xyz, v) + m.m1.w;
    result.z = dot(m.m2.xyz, v) + m.m2.w;
    return result;
}

float4 transformVector4(mat4 m, float4 v)
{
    float4 result;
    result.x = dot(m.m0, v);
    result.y = dot(m.m1, v);
    result.z = dot(m.m2, v);
	result.w = dot(m.m3, v); 
    return result;
}

bool solveLinearSystem2x2(mat2 A, float2 B, float2* x)
{ 
	float det = computeMat2Det(A);
	if (fabs(det) < 1e-8f)
		return false;

	
	*x = (float2)((A.m1.y * B.x - A.m0.y * B.y) / det,
                  (A.m0.x * B.y - A.m1.x * B.x) / det);
	
	return true;
}

#endif