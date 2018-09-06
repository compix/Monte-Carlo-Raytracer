#ifndef GEOMETRY_CL
#define GEOMETRY_CL

#include <math.cl>
#include <matrix.cl>
#include <kernel_data.h>
#include <materials.cl>

void computeTrianglePartialDerivates(float2 uv0, float2 uv1, float2 uv2, float3 p0, float3 p1, float3 p2, float3 normal, float3* dpdu, float3* dpdv)
{ 
	float2 duv02 = uv0 - uv2;
	float2 duv12 = uv1 - uv2;
	float3 dp02 = p0 - p2;
	float3 dp12 = p1 - p2;
	float det = duv02.x * duv12.y - duv02.y * duv12.x;

	if (isNotNearZero(det))
	{
		float invdet = 1.0f / det;
		*dpdu  = (duv12.y * dp02 - duv02.y * dp12) * invdet;
		*dpdv = -(-duv12.x * dp02 + duv02.x * dp12) * invdet;
	}
	else
	{
		*dpdu = normalize(computeOrthogonalVector(normal));
		*dpdv = normalize(cross(normal, *dpdu));
	}
}

void getRTShapeGeometryAttributes(const Scene* scene, int shapeIdx, int primIdx, float2 barycentrics, 
				                  float3* outPos, float2* outUV, float3* outNormal, float3* outTangent, float3* outBinormal)
{
	RTShape shape = scene->shapes[shapeIdx];

    unsigned int i0 = scene->indices[shape.startIdx + 3 * primIdx];
    unsigned int i1 = scene->indices[shape.startIdx + 3 * primIdx + 1];
    unsigned int i2 = scene->indices[shape.startIdx + 3 * primIdx + 2];

	float3 p0 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i0]);
	float3 p1 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i1]);
	float3 p2 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i2]);

	float2 uv0 = scene->uvs[shape.startVertex +i0];
	float2 uv1 = scene->uvs[shape.startVertex +i1];
	float2 uv2 = scene->uvs[shape.startVertex +i2];

	float3 n0 = transformVector3(shape.toWorldTransform, scene->normals[shape.startVertex +i0]);
	float3 n1 = transformVector3(shape.toWorldTransform, scene->normals[shape.startVertex +i1]);
	float3 n2 = transformVector3(shape.toWorldTransform, scene->normals[shape.startVertex +i2]);

	float3 t0 = transformVector3(shape.toWorldTransform, scene->tangents[shape.startVertex +i0]);
	float3 t1 = transformVector3(shape.toWorldTransform, scene->tangents[shape.startVertex +i1]);
	float3 t2 = transformVector3(shape.toWorldTransform, scene->tangents[shape.startVertex +i2]);

	float3 bn0 = transformVector3(shape.toWorldTransform, scene->binormals[shape.startVertex +i0]);
	float3 bn1 = transformVector3(shape.toWorldTransform, scene->binormals[shape.startVertex +i1]);
	float3 bn2 = transformVector3(shape.toWorldTransform, scene->binormals[shape.startVertex +i2]);

	*outPos = p0 * (1.0f - barycentrics.x - barycentrics.y) + p1 * barycentrics.x + p2 * barycentrics.y;
	*outUV = uv0 * (1.0f - barycentrics.x - barycentrics.y) + uv1 * barycentrics.x + uv2 * barycentrics.y;
	*outNormal = normalize(n0 * (1.0f - barycentrics.x - barycentrics.y) + n1 * barycentrics.x + n2 * barycentrics.y);
	*outTangent = normalize(t0 * (1.0f - barycentrics.x - barycentrics.y) + t1 * barycentrics.x + t2 * barycentrics.y);
	*outBinormal = normalize(bn0 * (1.0f - barycentrics.x - barycentrics.y) + bn1 * barycentrics.x + bn2 * barycentrics.y);
}

void getRTShapeUVs(const Scene* scene, int shapeIdx, int primIdx, float2* uv0, float2* uv1, float2* uv2)
{ 
	RTShape shape = scene->shapes[shapeIdx];

    unsigned int i0 = scene->indices[shape.startIdx + 3 * primIdx];
    unsigned int i1 = scene->indices[shape.startIdx + 3 * primIdx + 1];
    unsigned int i2 = scene->indices[shape.startIdx + 3 * primIdx + 2];

	*uv0 = scene->uvs[shape.startVertex +i0];
	*uv1 = scene->uvs[shape.startVertex +i1];
	*uv2 = scene->uvs[shape.startVertex +i2];
}

void getRTShapePositions(const Scene* scene, int shapeIdx, int primIdx, float3* p0, float3* p1, float3* p2)
{ 
	RTShape shape = scene->shapes[shapeIdx];

    unsigned int i0 = scene->indices[shape.startIdx + 3 * primIdx];
    unsigned int i1 = scene->indices[shape.startIdx + 3 * primIdx + 1];
    unsigned int i2 = scene->indices[shape.startIdx + 3 * primIdx + 2];

	*p0 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i0]);
	*p1 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i1]);
	*p2 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i2]);
}

inline RTInteraction computeSurfaceInteractionWithDifferentials(const Scene* scene, const RTIntersection* isect, __global const RTRayDifferentials* rayDifferentials)
{ 
	RTInteraction si;

	RTShape shape = scene->shapes[isect->shapeid];
	float2 barycentrics = (float2)(isect->uvwt.x, isect->uvwt.y);

	int primIdx = isect->primid;
    unsigned int i0 = scene->indices[shape.startIdx + 3 * primIdx];
    unsigned int i1 = scene->indices[shape.startIdx + 3 * primIdx + 1];
    unsigned int i2 = scene->indices[shape.startIdx + 3 * primIdx + 2];

	float3 p0 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i0]);
	float3 p1 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i1]);
	float3 p2 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i2]);

	float2 uv0 = scene->uvs[shape.startVertex +i0];
	float2 uv1 = scene->uvs[shape.startVertex +i1];
	float2 uv2 = scene->uvs[shape.startVertex +i2];

	float3 n0 = transformVector3(shape.toWorldInverseTranspose, scene->normals[shape.startVertex +i0]);
	float3 n1 = transformVector3(shape.toWorldInverseTranspose, scene->normals[shape.startVertex +i1]);
	float3 n2 = transformVector3(shape.toWorldInverseTranspose, scene->normals[shape.startVertex +i2]);

	si.p = p0 * (1.0f - barycentrics.x - barycentrics.y) + p1 * barycentrics.x + p2 * barycentrics.y;
	si.uv = uv0 * (1.0f - barycentrics.x - barycentrics.y) + uv1 * barycentrics.x + uv2 * barycentrics.y;
	si.gn = normalize(cross(p0 - p2, p1 - p2));
	si.sn = normalize(n0 * (1.0f - barycentrics.x - barycentrics.y) + n1 * barycentrics.x + n2 * barycentrics.y);

	computeTrianglePartialDerivates(uv0, uv1, uv2, p0, p1, p2, si.sn, &si.dpdu, &si.dpdv);

	// Compute shading partial derivatives - orthonormalization of dpdu, dpdv with shading normal si.sn
	si.sdpdu = normalize(si.dpdu - dot(si.sn, si.dpdu) * si.sn);
	si.sdpdv = normalize(si.dpdv - dot(si.sn, si.dpdv) * si.sn - dot(si.sdpdu, si.dpdv) * si.sdpdu);

	// *** Compute differentials: dpdx, dpdy, duvdx, duvdy ***
	// Compute plane intersection points px, py
	float d = dot(si.gn, si.p);
	float tx = -(dot(si.gn, rayDifferentials->xOrigin) - d) / dot(si.gn, rayDifferentials->xDirection);
	float3 px = rayDifferentials->xOrigin + tx * rayDifferentials->xDirection;
	float ty = -(dot(si.gn, rayDifferentials->yOrigin) - d) / dot(si.gn, rayDifferentials->yDirection);
	float3 py = rayDifferentials->yOrigin + ty * rayDifferentials->yDirection;

	si.dpdx = px - si.p;
	si.dpdy = py - si.p;
	// Compute uv offsets
	mat2 A;
	float2 Bx;
	float2 By;
	
	if (fabs(si.gn.x) > fabs(si.gn.y) && fabs(si.gn.x) > fabs(si.gn.z))
	{
		A = makeMat2(si.dpdu.y, si.dpdv.y,
					 si.dpdu.z, si.dpdv.z);
		Bx = px.yz - si.p.yz;
		By = py.yz - si.p.yz;
	}
	else if (fabs(si.gn.y) > fabs(si.gn.z))
	{ 
		A = makeMat2(si.dpdu.x, si.dpdv.x,
					 si.dpdu.z, si.dpdv.z);
		Bx = px.xz - si.p.xz;
		By = py.xz - si.p.xz;
	}
	else
	{ 
		A = makeMat2(si.dpdu.x, si.dpdv.x,
					 si.dpdu.y, si.dpdv.y);
		Bx = px.xy - si.p.xy;
		By = py.xy - si.p.xy;
	}
	
	if (!solveLinearSystem2x2(A, Bx, &si.duvdx))
		si.duvdx = (float2)(0.0f);
	
	if (!solveLinearSystem2x2(A, By, &si.duvdy))
		si.duvdy = (float2)(0.0f);

	// ******

	si.shapeIdx = isect->shapeid;

	return si;
}

inline RTInteraction computeSurfaceInteraction(const Scene* scene, const RTIntersection* isect)
{ 
	RTInteraction si;

	RTShape shape = scene->shapes[isect->shapeid];
	float2 barycentrics = (float2)(isect->uvwt.x, isect->uvwt.y);

	int primIdx = isect->primid;
    unsigned int i0 = scene->indices[shape.startIdx + 3 * primIdx];
    unsigned int i1 = scene->indices[shape.startIdx + 3 * primIdx + 1];
    unsigned int i2 = scene->indices[shape.startIdx + 3 * primIdx + 2];

	float3 p0 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i0]);
	float3 p1 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i1]);
	float3 p2 = transformPoint3(shape.toWorldTransform, scene->positions[shape.startVertex +i2]);

	float2 uv0 = scene->uvs[shape.startVertex +i0];
	float2 uv1 = scene->uvs[shape.startVertex +i1];
	float2 uv2 = scene->uvs[shape.startVertex +i2];

	float3 n0 = transformVector3(shape.toWorldInverseTranspose, scene->normals[shape.startVertex +i0]);
	float3 n1 = transformVector3(shape.toWorldInverseTranspose, scene->normals[shape.startVertex +i1]);
	float3 n2 = transformVector3(shape.toWorldInverseTranspose, scene->normals[shape.startVertex +i2]);

	si.p = p0 * (1.0f - barycentrics.x - barycentrics.y) + p1 * barycentrics.x + p2 * barycentrics.y;
	si.uv = uv0 * (1.0f - barycentrics.x - barycentrics.y) + uv1 * barycentrics.x + uv2 * barycentrics.y;
	si.gn = normalize(cross(p0 - p2, p1 - p2));
	si.sn = normalize(n0 * (1.0f - barycentrics.x - barycentrics.y) + n1 * barycentrics.x + n2 * barycentrics.y);

	computeTrianglePartialDerivates(uv0, uv1, uv2, p0, p1, p2, si.sn, &si.dpdu, &si.dpdv);

	// Compute shading partial derivatives - orthonormalization of dpdu, dpdv with shading normal si.sn
	si.sdpdu = normalize(si.dpdu - dot(si.sn, si.dpdu) * si.sn);
	si.sdpdv = normalize(si.dpdv - dot(si.sn, si.dpdv) * si.sn - dot(si.sdpdu, si.dpdv) * si.sdpdu);

	si.shapeIdx = isect->shapeid;

	return si;
}

#endif