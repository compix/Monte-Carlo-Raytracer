#ifndef MATERIALS_CL
#define MATERIALS_CL

/**
* Materials describe the surface properties of a shape for BTDF and BRDF computations.
*/

#include <bxdfs.cl>
#include <kernel_data.h>
#include <textures.cl>

#define USE_NORMAL_MAPPING

void applyNormalMapping_internal(int texId, __global const TextureDesc2D* textures, __global const uchar* normalMapData, RTInteraction* si)
{ 
#ifdef USE_NORMAL_MAPPING
 	float3 nm = 2.0f * readTexture2Df(texId, textures, normalMapData, si).xyz - 1.0f;
	si->sn = normalize(si->sdpdu * nm.x + si->sdpdv * nm.y + si->sn * nm.z);
	si->sdpdu = normalize(cross(si->sn, si->sdpdv));
	si->sdpdv = normalize(cross(si->sdpdu, si->sn));
#endif
}

inline void applyNormalMapping(const Scene* scene, int materialIdx, RTInteraction* si)
{
	if (materialIdx != RT_INVALID_ID && scene->materials[materialIdx].uber_normalMapId != RT_INVALID_ID)
	{ 
		applyNormalMapping_internal(scene->materials[materialIdx].uber_normalMapId, scene->textures2D, scene->texData2D, si);
	}
}

/**
* Computes lambertian brdf if sigma is 0 otherwise
* Oren Nayar brdf.
*/
float3 evaluateMatteMaterial(float3 Kd, float sigma, float3 woWorld, float3 wiWorld, const RTInteraction* si)
{ 
	if (!isReflection(woWorld, wiWorld, si))
		return (float3)(0.0f);

	float3 wo = dirToShadingSpace(woWorld, si);
	float3 wi = dirToShadingSpace(wiWorld, si);

	if (sigma == 0.0f)
		return evaluateLambertianReflection(Kd);
	else
		return evaluateOrenNayar(Kd, sigma, wo, wi);
}

float3 evaluatePlasticMaterial(float3 Kd, float3 Ks, float roughness, float3 woWorld, float3 wiWorld, const RTInteraction* si)
{ 
	if (!isReflection(woWorld, wiWorld, si))
		return (float3)(0.0f);

	float3 wo = dirToShadingSpace(woWorld, si);
	float3 wi = dirToShadingSpace(wiWorld, si);

	return evaluateLambertianReflection(Kd) +
		   evaluateMicrofacetReflection_TorranceSparrow_Dielectric_TrowbridgeReitz(Ks, (float2)(roughness), 1.0f, 1.5f, wo, wi);
}

//float3 evaluateGlassMaterial(float3 Kr, float3 Kt, float2 roughness, float idxOfRefraction, float bump, float3 woWorld, float3 wiWorld, RTInteraction si)
//{ 
//	
//}

typedef struct
{ 
	float3 Kd, Ks, Kr, opacity;
	float4 Kt;
	float2 roughness;
	float eta;
	float pad;
} RTUberMaterialProperties;

inline void getUberMaterialProperties(const Scene* scene, int materialIdx, const RTInteraction* si, RTUberMaterialProperties* properties)
{ 
	RTMaterial material = scene->materials[materialIdx];

	float4 Kd_opacity = readTexture2Df_ifValid(material.uber_diffuseTexId, scene->textures2D, scene->texData2D, si, (float4)(1.0f, 1.0f, 1.0f, 1.0f));
	properties->Kd = Kd_opacity.xyz * material.uber_kd;
	properties->Ks = readTexture2Df3_ifValid(material.uber_glossyTexId, scene->textures2D, scene->texData2D, si, (float3)(1.0f, 1.0f, 1.0f)) * material.uber_ks;
	properties->Kr = readTexture2Df3_ifValid(material.uber_specReflectionTexId, scene->textures2D, scene->texData2D, si, (float3)(1.0f, 1.0f, 1.0f)) * material.uber_kr;
	properties->Kt.xyz = readTexture2Df3_ifValid(material.uber_transmissionTexId, scene->textures2D, scene->texData2D, si, (float3)(1.0f, 1.0f, 1.0f)) * material.uber_kt.xyz;
	properties->Kt.w = material.uber_kt.w;
	properties->opacity = readTexture2Df3_ifValid(material.uber_opacityTexId, scene->textures2D, scene->texData2D, si, (float3)(1.0f, 1.0f, 1.0f)) * material.uber_opacity * Kd_opacity.w;
	properties->roughness = readTexture2Df2_ifValid(material.uber_roughnessTexId, scene->textures2D, scene->texData2D, si, material.uber_roughness);
	properties->eta = readTexture2Df1_ifValid(material.uber_iorTexId, scene->textures2D, scene->texData2D, si, material.uber_eta);

	properties->roughness = (float2)(roughnessToAlpha(properties->roughness.x), roughnessToAlpha(properties->roughness.y));
}

float evaluateUberMaterialPdf(const Scene* scene, int materialIdx, float3 woWorld, float3 wiWorld, const RTInteraction* si, BxDFType type)
{ 
	RTUberMaterialProperties um;
	getUberMaterialProperties(scene, materialIdx, si, &um);
	
	return evaluateUberBSDF_Pdf(um.Kd, um.Ks, um.Kr, um.Kt, um.roughness, um.opacity, um.eta, si, woWorld, wiWorld, type);
}

float3 evaluateUberMaterial(const Scene* scene, int materialIdx, float3 woWorld, float3 wiWorld, const RTInteraction* si, TransportMode mode)
{ 
	RTUberMaterialProperties um;
	getUberMaterialProperties(scene, materialIdx, si, &um);

	return evaluateUberBSDF(um.Kd, um.Ks, um.Kt, um.roughness, um.opacity, um.eta, si, woWorld, wiWorld, mode);
}

float3 sampleUberMaterial(const Scene* scene, int materialIdx, const RTInteraction* si, float2 u, TransportMode mode, 
						  BxDFType type, float3 woWorld, float3* wiWorld, float* pdf, int* numNonDeltaTypes, BxDFType* sampledType)
{
	RTUberMaterialProperties um;
	getUberMaterialProperties(scene, materialIdx, si, &um);

	return sampleUberBSDF(um.Kd, um.Ks, um.Kr, um.Kt, um.roughness, um.opacity, um.eta, si, u, mode, type, woWorld, wiWorld, pdf, numNonDeltaTypes, sampledType);
}

float evaluateMaterialPdf(const Scene* scene, int materialIdx, float3 woWorld, float3 wiWorld, const RTInteraction* si, BxDFType type)
{ 
	switch (scene->materials[materialIdx].type)
	{
	case RT_UBER_MATERIAL:
		{ 
			return evaluateUberMaterialPdf(scene, materialIdx, woWorld, wiWorld, si, type);
		}
	}

	return 0.0f;
}

float3 evaluateMaterial(const Scene* scene, int materialIdx, float3 woWorld, float3 wiWorld, const RTInteraction* si, TransportMode mode)
{ 
	switch (scene->materials[materialIdx].type)
	{
	case RT_UBER_MATERIAL:
		{ 
			return evaluateUberMaterial(scene, materialIdx, woWorld, wiWorld, si, mode);
		}
	}

	return (float3)(0.0f, 0.0f, 0.0f);
}

float3 sampleMaterial(const Scene* scene, int materialIdx, const RTInteraction* si, float2 u, TransportMode mode, 
					  BxDFType type, float3 woWorld, float3* wiWorld, float* pdf, int* numNonDeltaTypes, BxDFType* sampledType)
{ 
	switch (scene->materials[materialIdx].type)
	{
	case RT_UBER_MATERIAL:
		{ 
			return sampleUberMaterial(scene, materialIdx, si, u, mode, type, woWorld, wiWorld, pdf, numNonDeltaTypes, sampledType);
		}
	}

	return (float3)(0.0f, 0.0f, 0.0f);
}

/**
* It might be a good idea to cache this info per material. Especially the texture fetches.
*/
inline bool hasMaterialNonDeltaComponents(const Scene* scene, int materialIdx, const RTInteraction* si)
{
	RTMaterial material = scene->materials[materialIdx];

	switch (material.type)
	{
	case RT_UBER_MATERIAL:
		{ 
			float3 Kd = readTexture2Df3_ifValid(material.uber_diffuseTexId, scene->textures2D, scene->texData2D, si, material.uber_kd);
			float3 Ks = readTexture2Df3_ifValid(material.uber_glossyTexId, scene->textures2D, scene->texData2D, si, material.uber_ks);
			float3 opacity = readTexture2Df3_ifValid(material.uber_opacityTexId, scene->textures2D, scene->texData2D, si, material.uber_opacity);
			float3 kd = Kd * opacity;
			float3 ks = Ks * opacity;
			return (!isBlack(kd) || !isBlack(ks));
		}
	}

	return false;
}


#endif