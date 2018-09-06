#ifndef BXDFS_CL
#define BXDFS_CL

/**
* Implementations based on "Physically Based Rendering From Theory to Implementation Third Edition".

Coventions for the shading coordinate system:
- Incident and outgoing directions wi, wo are always facing outward and they are normalized.
- Surface normal is facing outside of the object.
- BxDF implementations must compute a result even if wo, wi are in different hemispheres.
  -> This leads to the abs(cos(theta)) instead of the usual max(0, cos(theta)) term.
*/

#include <math.cl>
#include <samplers.cl>

/** Operations defined in the shading coordinate system
* Normal (up) - y-axis
* Tangent (right) - x-axis
* Binormal (forward) - z-axis
*/
inline float evalCosTheta(float3 w) { return w.y; }
inline float evalCosSqTheta(float3 w) { return w.y * w.y; }
inline float evalAbsCosTheta(float3 w) { return fabs(w.y); }
inline float evalSinSqTheta(float3 w) { return fmax(0.0f, 1.0f - evalCosSqTheta(w)); }
inline float evalSinTheta(float3 w) { return sqrt(evalSinSqTheta(w)); }
inline float evalTanTheta(float3 w) { return evalSinTheta(w) / evalCosTheta(w); }
inline float evalTanSqTheta(float3 w) { return evalSinSqTheta(w) / evalCosSqTheta(w); }
inline float evalCosPhi(float3 w)
{ 
	float st = evalSinTheta(w);
	return st == 0 ? 1.0f : clamp(w.x / st, -1.0f, 1.0f);
}

inline float evalSinPhi(float3 w)
{ 
	float st = evalSinTheta(w);
	return evalSinTheta == 0 ? 0 : clamp(w.z / st, -1.0f, 1.0f);
}

inline float evalCosSqPhi(float3 w)
{ 
	return evalCosPhi(w) * evalCosPhi(w);
}

inline float evalSinSqPhi(float3 w)
{ 
	return evalSinPhi(w) * evalSinPhi(w);
}

inline float evalCosDPhi(float3 wa, float3 wb)
{ 
	return clamp((wa.x * wb.x + wa.z * wb.z) / sqrt((wa.x * wa.x + wa.z * wa.z) * (wb.x * wb.x + wb.z * wb.z)), -1.0f, 1.0f);
}

inline bool isSameHemisphere(float3 v0, float3 v1)
{ 
	return v0.y * v1.y > 0.0f;
}

#define BLACK_COLOR_COMPONENT_EPSILON 0.000001f

inline bool isBlack(float3 color)
{ 
	return color.x < BLACK_COLOR_COMPONENT_EPSILON && color.y < BLACK_COLOR_COMPONENT_EPSILON && color.z < BLACK_COLOR_COMPONENT_EPSILON;
}

inline bool isNotBlack(float3 color)
{ 
	return color.x > BLACK_COLOR_COMPONENT_EPSILON || color.y > BLACK_COLOR_COMPONENT_EPSILON || color.z > BLACK_COLOR_COMPONENT_EPSILON;
}

/**
* Given the orthonormal basis 
* n: normal
* t: tangent
* bn: binormal
* the function transforms w to the shading coordinate system.
* @return w in the world space
*/
inline float3 transformDirectionVectorToShadingSpace(float3 w, float3 n, float3 t, float3 bn)
{ 
	return (float3)(dot(t, w), dot(n, w), dot(bn, w));
}

/**
* Given the orthonormal basis 
* n: normal
* t: tangent
* bn: binormal
* the function transforms w from the shading to the world coordinate system.
* @return w in the shading space
*/
inline float3 transformDirectionVectorFromShadingToWorldSpace(float3 w, float3 n, float3 t, float3 bn)
{ 
	return (float3)(t.x * w.x + n.x * w.y + bn.x * w.z,
					t.y * w.x + n.y * w.y + bn.y * w.z,
					t.z * w.x + n.z * w.y + bn.z * w.z);
}

/**
* Convenience function that transforms a vector into the shading space with a given RTInteraction.
*/
inline float3 dirToShadingSpace(float3 v, const RTInteraction* si)
{ 
	return transformDirectionVectorToShadingSpace(v, si->sn, si->sdpdu, si->sdpdv);
}

inline float3 dirFromShadingToWorld(float3 v, const RTInteraction* si)
{ 
	return transformDirectionVectorFromShadingToWorldSpace(v, si->sn, si->sdpdu, si->sdpdv);
}

/**
* Returns true if woWorld, wiWorld are in the same hemisphere formed by geometry normal si.gn
* otherwise false is returned and transmission should be considered.
*/
inline bool isReflection(float3 woWorld, float3 wiWorld, const RTInteraction* si)
{ 
	return dot(si->gn, woWorld) * dot(si->gn, wiWorld) > 0.0f;
}

typedef enum
{ 
	BSDF_NONE         = 0,
	BSDF_REFLECTION   = (1 << 0),
	BSDF_TRANSMISSION = (1 << 1),
	BSDF_DIFFUSE      = (1 << 2),
	BSDF_GLOSSY		  = (1 << 3),
	BSDF_SPECULAR	  = (1 << 4),
	BSDF_SPECULAR_REFLECTION = (BSDF_REFLECTION | BSDF_SPECULAR),
	BSDF_SPECULAR_TRANSMISSION = (BSDF_TRANSMISSION | BSDF_SPECULAR),
	BSDF_FRESNEL_SPECULAR = (BSDF_REFLECTION | BSDF_TRANSMISSION | BSDF_SPECULAR),
	BSDF_LAMBERTIAN_REFLECTION = (BSDF_REFLECTION | BSDF_DIFFUSE),
	BSDF_OREN_NAYAR_REFLECTION = (BSDF_REFLECTION | BSDF_DIFFUSE),
	BSDF_MICROFACET_REFLECTION = (BSDF_REFLECTION | BSDF_GLOSSY),
	BSDF_MICROFACET_TRANSMISSION = (BSDF_TRANSMISSION | BSDF_GLOSSY),
	BSDF_FRESNEL_BLEND = (BSDF_REFLECTION | BSDF_GLOSSY),
	BSDF_NON_DELTA_FLAGS = (BSDF_REFLECTION | BSDF_TRANSMISSION | BSDF_DIFFUSE | BSDF_GLOSSY),
	BSDF_ALL		  = (BSDF_DIFFUSE | BSDF_GLOSSY | BSDF_SPECULAR | BSDF_REFLECTION | BSDF_TRANSMISSION)
} BxDFType;

typedef enum
{ 
	TRANSPORT_MODE_RADIANCE,
	TRANSPORT_MODE_IMPORTANCE
} TransportMode;

inline bool BxDFMatchesFlags(BxDFType bxdfType, BxDFType flags)
{ 
	return (bxdfType & flags) == bxdfType;
}

inline float3 evaluateFresnelSchlick(float3 Rs, float in_cosTheta)
{
	return Rs + pow(1.0f - in_cosTheta, 5) * ((float3)(1.0f) - Rs);
}

float evaluateFresnelDielectric(float cosThetaI, float etaI, float etaT)
{ 
	cosThetaI = clamp(cosThetaI, -1.0f, 1.0f);
	
	// Indices of refraction need to be swapped if the
	// incident ray is inside the medium (below the hemisphere of the surface)
	if (cosThetaI <= 0.0f)
	{
		float h = etaI;
		etaI = etaT;
		etaT = h;
		
		cosThetaI = fabs(cosThetaI);
	}

	// Snell's law allows us to compute cosThetaT.
	float sinThetaI = sqrt(fmax(0.0f, 1.0f - cosThetaI * cosThetaI));
	float sinThetaT = etaI / etaT * sinThetaI;

	// Handle total internal reflection
	if (sinThetaT >= 1.0f)
		return 1.0f;

	float cosThetaT = sqrt(fmax(0.0f, 1.0f - sinThetaT * sinThetaT));

	float rparl = ((etaT * cosThetaI) - (etaI * cosThetaT)) /
				  ((etaT * cosThetaI) + (etaI * cosThetaT));
	float rperp = ((etaI * cosThetaI) - (etaT * cosThetaT)) /
				  ((etaI * cosThetaI) + (etaT * cosThetaT));

	return (rparl * rparl + rperp * rperp) * 0.5f;
}

/**
* Expecting cosThetaI to be positive
* @param k absorption coefficient
* Source: https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
*/
float3 evaluateFresnelConductor(float cosThetaI, float3 etai, float3 etat, float3 k) 
{
    cosThetaI = clamp(cosThetaI, -1.0f, 1.0f);
    float3 eta = etat / etai;
    float3 etak = k / etai;

    float cosThetaI2 = cosThetaI * cosThetaI;
    float sinThetaI2 = 1.0f - cosThetaI2;
    float3 eta2 = eta * eta;
    float3 etak2 = etak * etak;

    float3 t0 = eta2 - etak2 - sinThetaI2;
    float3 a2plusb2 = sqrt(t0 * t0 + 4.0f * eta2 * etak2);
    float3 t1 = a2plusb2 + cosThetaI2;
    float3 a = sqrt(0.5f * (a2plusb2 + t0));
    float3 t2 = 2.0f * cosThetaI * a;
    float3 Rs = (t1 - t2) / (t1 + t2);

    float3 t3 = cosThetaI2 * a2plusb2 + sinThetaI2 * sinThetaI2;
    float3 t4 = t2 * sinThetaI2;
    float3 Rp = Rs * (t3 - t4) / (t3 + t4);

    return 0.5f * (Rp + Rs);
}

/**
* @param wo Outgoing vector.
* @param n Normal vector.
* Note: This computation expects the vectors to point outward and thus differs from glsl, hlsl reflect functions.
*       Just negate wo to achieve the same result.
*/
inline float3 reflect(float3 wo, float3 n)
{ 
	return -wo + 2.0f * dot(n, wo) * n;
}

inline bool refract(float3 wi, float3 n, float eta, float3* wt)
{ 
	float cosThetaI = dot(n, wi);
	float sin2ThetaI = fmax(0.0f, 1.0f - cosThetaI * cosThetaI);
	float sin2ThetaT = eta * eta * sin2ThetaI;

	if (sin2ThetaT >= 1.0f)
		return false;

	float cosThetaT = sqrt(1.0f - sin2ThetaT);
	*wt = -eta * wi + (eta * cosThetaI - cosThetaT) * n;
	return true;
}

inline float3 refract_fast(float3 wi, float3 n, float eta)
{
	float sinSqThetaT = eta * eta * (1.0f - dot(n, wi) * dot(n, wi));
	if (sinSqThetaT >= 1.0f)
		return (float3)(0.0f);

	return -eta * wi + (eta * dot(n, wi) - sqrt(1.0f - sinSqThetaT)) * n;
}

/**
* Computes perfect mirror-reflection.
*/
float3 sampleSpecularReflection_Dielectric(float3 R, float etaI, float etaT, float3 wo, float3* wi, float* pdf)
{ 
	// Compute perfect specular reflection direction.
	// Instead of using reflect() the computation can be simplified since 
	// n = (0,1,0) in our shading coordinate system.
	*wi = (float3)(-wo.x, wo.y, -wo.z);
	*pdf = 1.0f;
	float F = evaluateFresnelDielectric(evalCosTheta(*wi), etaI, etaT);
	return F * R / evalAbsCosTheta(*wi);
}

/**
* Computes perfect mirror-reflection.
*/
float3 sampleSpecularReflection_Conductor(float3 R, float3 etaI, float3 etaT, float3 k, float3 wo, float3* wi, float* pdf)
{ 
	// Compute perfect specular reflection direction.
	// Instead of using reflect() the computation can be simplified since 
	// n = (0,1,0) in our shading coordinate system.
	*wi = (float3)(-wo.x, wo.y, -wo.z);
	*pdf = 1.0f;
	float3 F = evaluateFresnelConductor(evalCosTheta(*wi), etaI, etaT, k);
	return F * R / evalAbsCosTheta(*wi);
}

/**
* @param etaA Index of refraction above the surface.
* @param etaB Index of refraction below the surface (e.g. inside the mesh).
*/
float3 sampleSpecularTransmission(float3 T, float etaA, float etaB, TransportMode mode, float3 wo, float3* wi, float* pdf)
{ 
	// Figure out which eta is incident and which is transmitted
	bool isEntering = evalCosTheta(wo) > 0.0f;
	float etaI = isEntering ? etaA : etaB;
	float etaT = isEntering ? etaB : etaA;

	// Compute ray direction for specular transmission
	float3 n = (float3)(0.0f, 1.0f, 0.0f) * sign(wo.y);
	if (!refract(wo, n, etaI / etaT, wi))
		return (float3)(0.0f, 0.0f, 0.0f);

	*pdf = 1.0f;
	float3 ft = T * (1.0f - evaluateFresnelDielectric(evalCosTheta(*wi), etaA, etaB));
	// Account for non-symmetry with transmission to different medium
	if (mode == TRANSPORT_MODE_RADIANCE) 
		ft *= (etaI * etaI) / (etaT * etaT);

	return ft / evalAbsCosTheta(*wi);
}

float3 evaluateFresnelSpecular()
{ 
	return (float3)(0.0f);
}

/**
* Models a perfect diffuse surface. PBR page 532
*/
void sampleCosineHemisphere(float2 u, float3 wo, float3* wi, float* pdf)
{ 
	*wi = cosineSampleHemisphere(u);
	if (wo.y < 0.0f)
	{
		float3 wiTemp = *wi;
		wiTemp.y *= -1.0f;
		*wi = wiTemp;
	}

	*pdf = evalAbsCosTheta(*wi) * PI_INV;
}

inline float evaluateLambertianReflectionPdf(float3 wo, float3 wi)
{ 
	return isSameHemisphere(wo, wi) ? evalAbsCosTheta(wi) * PI_INV : 0.0f;
}

inline float3 evaluateLambertianReflection(float3 R)
{ 
	return R * PI_INV;
}

float3 sampleLambertianReflection(float3 R, float2 u, float3 wo, float3* wi, float* pdf)
{ 
	//*wi = uniformSampleHemisphere(u);
	//*pdf = uniformHemispherePdf();

	sampleCosineHemisphere(u, wo, wi, pdf);
	return R * PI_INV;
}

/**
* @param sigma Standard deviation of the microfacet orientation angle in radians.
*/
float3 evaluateOrenNayar(float3 R, float sigma, float3 wo, float3 wi)
{ 
	float sigma2 = sigma * sigma;
	float A = 1.0f - (sigma2 / (2.0f * (sigma2 + 0.33f)));
	float B = 0.45f * sigma2 / (sigma2 + 0.09f);
	
	float sinThetaI = evalSinTheta(wi);
	float sinThetaO = evalSinTheta(wo);

	// Compute cos term of Oren-Nayar model
	float maxCos = 0.0f;
	if (sinThetaI > 1e-4 && sinThetaO > 1e-4)
	{
		float dCos = evalCosPhi(wi) * evalCosPhi(wo) + evalSinPhi(wi) * evalSinPhi(wo);
		maxCos = fmax(0.0f, dCos);
	}

	// Compute sin and tangent terms of Oren-Nayar model
	float sinAlpha, tanBeta;
	if (evalAbsCosTheta(wi) > evalAbsCosTheta(wo)) 
	{
		sinAlpha = sinThetaO;
		tanBeta = sinThetaI / evalAbsCosTheta(wi);
	} 
	else
	{
		sinAlpha = sinThetaI;
		tanBeta = sinThetaO / evalAbsCosTheta(wo);
	}

	return R * PI_INV * (A + B * maxCos * sinAlpha * tanBeta);
}

float roughnessToAlpha(float roughness) 
{
    roughness = fmax(roughness, 1e-3f);
    float x = log(roughness);
    return 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x + 0.000640711f * x * x * x * x;
}

/**
* Computes the differential area of microfacets with the surface normal wh with the Beckmann-Distribution.
*/
float computeBeckmannDistribution(float3 wh, float2 alpha)
{ 
	float ttan2Theta = evalTanSqTheta(wh);
	if (isinf(ttan2Theta)) 
		return 0.0f;

	float cos4Theta = evalCosSqTheta(wh) * evalCosSqTheta(wh);
	return exp(-ttan2Theta * (evalCosSqPhi(wh) / (alpha.x * alpha.x) +
			    evalSinSqPhi(wh) / (alpha.y * alpha.y))) / (PI * alpha.x * alpha.y * cos4Theta);
}

float computeTrowbridgeReitzDistribution(float3 wh, float2 alpha)
{ 
	float tan2Theta = evalTanSqTheta(wh);
	if (isinf(tan2Theta)) 
		return 0.0f;

	const float cos4Theta = evalCosSqTheta(wh) * evalCosSqTheta(wh);
	float e = (evalCosSqPhi(wh) / (alpha.x * alpha.x) + evalSinSqPhi(wh) / (alpha.y * alpha.y)) * tan2Theta;
	return 1.0f / (PI * alpha.x * alpha.y * cos4Theta * (1.0f + e) * (1.0f + e));
}

/**
* Measures invisible masked microfacet area per visible microfacet area.
*/
float computeBeckmannDistributionLambda(float3 w, float2 alpha)
{ 
	float absTanTheta = fabs(evalTanTheta(w));
	if (isinf(absTanTheta)) 
		return 0.0f;

	float alphaW = sqrt(evalCosSqPhi(w) * alpha.x * alpha.x + evalSinSqPhi(w) * alpha.y * alpha.y);

	float a = 1 / (alphaW * absTanTheta);
	if (a >= 1.6f)
		return 0.0f;

	return (1.0f - 1.259f * a + 0.396f * a * a) / (3.535f * a + 2.181f * a * a);
}

float computeTrowbridgeReitzDistributionLambda(float3 w, float2 alpha)
{ 
	float absTanTheta = fabs(evalTanTheta(w));
	if (isinf(absTanTheta)) 
		return 0.0f;

	float alphaW = sqrt(evalCosSqPhi(w) * alpha.x * alpha.x + evalSinSqPhi(w) * alpha.y * alpha.y);

	float alpha2Tan2Theta = (alphaW * absTanTheta) * (alphaW * absTanTheta);
	return (-1.0f + sqrt(1.f + alpha2Tan2Theta)) / 2.0f;
}

inline float computeBeckmannDistributionG1(float3 w, float2 alpha)
{ 
	return 1.0f / (1.0f + computeBeckmannDistributionLambda(w, alpha));
}

inline float computeTrowbridgeReitzDistributionG1(float3 w, float2 alpha)
{ 
	return 1.0f / (1.0f + computeTrowbridgeReitzDistributionLambda(w, alpha));
}

/**
* Computes the fraction of microfacets in a differential area that are visible from directions wo and wi.
* Pass lambdas by calling the appropriate functions for a particular microfacet distribution model.
*/
inline float computeG(float lambdawo, float lambdawi)
{ 
	return 1.0f / (1.0f + lambdawo + lambdawi);
}

inline float computeBeckmannDistributionG(float3 wo, float3 wi, float2 alpha)
{ 
	return computeG(computeBeckmannDistributionLambda(wo, alpha), computeBeckmannDistributionLambda(wi, alpha));
}

inline float computeTrowbridgeReitzDistributionG(float3 wo, float3 wi, float2 alpha)
{ 
	return computeG(computeTrowbridgeReitzDistributionLambda(wo, alpha), computeTrowbridgeReitzDistributionLambda(wi, alpha));
}

/**
* @param D The normal distribution term.
* @param G Shadowing-masking term.
* @param F Fresnel-term.
*/
float3 evaluateMicrofacetReflection_TorranceSparrow_Dielectric_TrowbridgeReitz(float3 R, float2 alpha, float etaI, float etaT, float3 wo, float3 wi)
{ 
	float cosThetaO = evalAbsCosTheta(wo);
	float cosThetaI = evalAbsCosTheta(wi);

	float3 wh = wi + wo;

	// Handle degenerate cases for microfacet reflection
	if (cosThetaI == 0.0f || cosThetaO == 0.0f) 
		return (float3)(0.0f);

	if (wh.x == 0.0f && wh.y == 0.0f && wh.z == 0.0f)
		return (float3)(0.0f);

	wh = normalize(wh);

	float F = evaluateFresnelDielectric(dot(wi, wh), etaI, etaT);

	return R * computeTrowbridgeReitzDistribution(wh, alpha) * computeTrowbridgeReitzDistributionG(wo, wi, alpha) * F / (4.0f * cosThetaI * cosThetaO);
}

/**
* @param D The normal distribution term.
* @param G Shadowing-masking term.
* @param F Fresnel-term.
*/
float3 evaluateMicrofacetReflection_TorranceSparrow_Dielectric_Beckmann(float3 R, float2 alpha, float etaI, float etaT, float3 wo, float3 wi)
{ 
	float cosThetaO = evalAbsCosTheta(wo);
	float cosThetaI = evalAbsCosTheta(wi);

	float3 wh = wi + wo;

	// Handle degenerate cases for microfacet reflection
	if (cosThetaI == 0.0f || cosThetaO == 0.0f) 
		return (float3)(0.0f);

	if (wh.x == 0.0f && wh.y == 0.0f && wh.z == 0.0f)
		return (float3)(0.0f);

	wh = normalize(wh);

	float F = evaluateFresnelDielectric(dot(wi, wh), etaI, etaT);

	return R * computeBeckmannDistribution(wh, alpha) * computeBeckmannDistributionG(wo, wi, alpha) * F / (4.0f * cosThetaI * cosThetaO);
}

/**
* Defaults to Dielectric_TrowbridgeReitz.
*/
float3 evaluateMicrofacetReflection_TorranceSparrow(float3 R, float2 alpha, float etaI, float etaT, float3 wo, float3 wi)
{ 
	return evaluateMicrofacetReflection_TorranceSparrow_Dielectric_TrowbridgeReitz(R, alpha, etaI, etaT, wo, wi);
}

float3 evaluateMicrofacetTransmission_TorranceSparrow_Beckmann(float3 T, TransportMode mode, float2 alpha, float etaI, float etaT, float3 wo, float3 wi) 
{
	// Consider only transmission
    if (isSameHemisphere(wo, wi)) 
		return (float3)(0.0f);

    float cosThetaO = evalCosTheta(wo);
    float cosThetaI = evalCosTheta(wi);
    if (cosThetaI == 0.0f || cosThetaO == 0.0f) 
		return (float3)(0.0f);

    // Compute wh from wo and wi for microfacet transmission
    float eta = evalCosTheta(wo) > 0.0f ? (etaT / etaI) : (etaI / etaT);
    float3 wh = normalize(wo + wi * eta);
    if (wh.z < 0) 
		wh = -wh;

    float F = evaluateFresnelDielectric(dot(wo, wh), etaI, etaT);

    float sqrtDenom = dot(wo, wh) + eta * dot(wi, wh);
    float factor = (mode == TRANSPORT_MODE_RADIANCE) ? (1.0f / eta) : 1.0f;

    return ((float3)(1.0f) - F) * T *
           fabs(computeBeckmannDistribution(wh, alpha) * computeBeckmannDistributionG(wo, wi, alpha) * eta * eta *
           absDot(wi, wh) * absDot(wo, wh) * factor * factor / (cosThetaI * cosThetaO * sqrtDenom * sqrtDenom));
}

float3 evaluateMicrofacetTransmission_TorranceSparrow_TrowbridgeReitz(float3 T, TransportMode mode, float2 alpha, float etaI, float etaT, float3 wo, float3 wi) 
{
	// Consider only transmission
    if (isSameHemisphere(wo, wi)) 
		return (float3)(0.0f);

    float cosThetaO = evalCosTheta(wo);
    float cosThetaI = evalCosTheta(wi);
    if (cosThetaI == 0.0f || cosThetaO == 0.0f) 
		return (float3)(0.0f);

    // Compute wh from wo and wi for microfacet transmission
    float eta = evalCosTheta(wo) > 0.0f ? (etaT / etaI) : (etaI / etaT);
    float3 wh = normalize(wo + wi * eta);
    if (wh.z < 0) 
		wh = -wh;

    float F = evaluateFresnelDielectric(dot(wo, wh), etaI, etaT);

    float sqrtDenom = dot(wo, wh) + eta * dot(wi, wh);
    float factor = (mode == TRANSPORT_MODE_RADIANCE) ? (1.0f / eta) : 1.0f;

    return ((float3)(1.0f) - F) * T *
           fabs(computeTrowbridgeReitzDistribution(wh, alpha) * computeTrowbridgeReitzDistributionG(wo, wi, alpha) * eta * eta *
           absDot(wi, wh) * absDot(wo, wh) * factor * factor / (cosThetaI * cosThetaO * sqrtDenom * sqrtDenom));
}

float3 evaluateFresnelBlend(float3 Rd, float3 Rs, float2 alpha, float3 wo, float3 wi)
{ 
	float3 wh = wi + wo;
	if (wh.x == 0.0f && wh.y == 0.0 && wh.z == 0.0f) 
		return (float3)(0.0f);

	float3 diffuse = (28.0f / (23.0f * PI)) * Rd * ((float3)(1.0f) - Rs) *
					 (1.0f - pow(1.0f - 0.5f * evalAbsCosTheta(wi), 5)) *
					 (1.0f - pow(1.0f - 0.5f * evalAbsCosTheta(wo), 5));

	wh = normalize(wh);
	float3 specular = computeBeckmannDistribution(wh, alpha) / (4.0f * absDot(wi, wh) *
					  fmax(evalAbsCosTheta(wi), evalAbsCosTheta(wo))) * evaluateFresnelSchlick(Rs, dot(wi, wh));

	return diffuse + specular;
}

/**
* Note: Currently this function samples the full distribution, not accounting for invisible area of microfacets.
*       There is a better way of doing it as shown in the pbrt code.
*/
float3 sampleBeckmannDistribution_wh(float2 u, float3 wo, float2 alpha)
{ 
    // Compute tan^2(theta) and phi for Beckmann distribution sample
    float tan2Theta, phi;
	// Isotropic
    if (alpha.x == alpha.y) 
	{
        float logSample = log(1.0f - u.x);
        tan2Theta = -alpha.x * alpha.x * logSample;
        phi = u.y * 2.0f * PI;
    } 
	else // Anisotropic
	{
        // Compute tan2Theta and phi for anisotropic Beckmann distribution
        float logSample = log(1.0f - u.x);
        phi = atan(alpha.y / alpha.x * tan(2.0f * PI * u.y + 0.5f * PI));
        if (u.y > 0.5f) 
			phi += PI;

        float sinPhi = sin(phi);
		float cosPhi = cos(phi);
        float alphax2 = alpha.x * alpha.x;
		float alphay2 = alpha.y * alpha.y;
        tan2Theta = -logSample / (cosPhi * cosPhi / alphax2 + sinPhi * sinPhi / alphay2);
    }

    // Map sampled Beckmann angles to normal direction _wh_
    float cosTheta = 1.0f / sqrt(1.0f + tan2Theta);
    float sinTheta = sqrt(fmax(0.0f, 1.0f - cosTheta * cosTheta));
    float3 wh = makeSphericalDirection(sinTheta, cosTheta, phi);
    if (!isSameHemisphere(wo, wh)) 
		wh = -wh;

	return wh;
}

float3 sampleTrowbridgeReitzDistribution_wh(float2 u, float3 wo, float2 alpha)
{
    float cosTheta = 0.0f;
	float phi = (2.0f * PI) * u.y;
    if (alpha.x == alpha.y) 
	{
        float tanTheta2 = alpha.x * alpha.x * u.x / (1.0f - u.x);
        cosTheta = 1.0f / sqrt(1.0f + tanTheta2);
    } 
	else 
	{
        phi = atan(alpha.y / alpha.x * tan(2.0f * PI * u.y + 0.5f * PI));
        if (u.y > .5f) 
			phi += PI;
        float sinPhi = sin(phi);
		float cosPhi = cos(phi);
        const float alphax2 = alpha.x * alpha.x, alphay2 = alpha.y * alpha.y;
        const float alpha2 = 1.0f / (cosPhi * cosPhi / alphax2 + sinPhi * sinPhi / alphay2);
        float tanTheta2 = alpha2 * u.x / (1.0f - u.x);
        cosTheta = 1.0f / sqrt(1.0f + tanTheta2);
    }

    float sinTheta = sqrt(fmax(0.0f, 1.0f - cosTheta * cosTheta));
    float3 wh = makeSphericalDirection(sinTheta, cosTheta, phi);
    if (!isSameHemisphere(wo, wh)) 
		wh = -wh;

    return wh;
}

inline float evalTrowbridgeReitzPdf_wh(float3 wo, float3 wh, float2 alpha)
{ 
	return computeTrowbridgeReitzDistribution(wh, alpha) * evalAbsCosTheta(wh);
}

inline float evalBeckmannPdf_wh(float3 wo, float3 wh, float2 alpha)
{ 
	return computeBeckmannDistribution(wh, alpha) * evalAbsCosTheta(wh);
}

inline float evalMicrofacetReflectionPdf_Beckmann(float3 wo, float3 wi, float3 wh, float2 alpha)
{ 
	if (!isSameHemisphere(wo, wi))
		return 0.0f;

	return evalBeckmannPdf_wh(wo, wh, alpha) / (4.0f * dot(wo, wh));
}

inline float evalMicrofacetReflectionPdf_TrowbridgeReitz(float3 wo, float3 wi, float3 wh, float2 alpha)
{ 
	if (!isSameHemisphere(wo, wi))
		return 0.0f;

	return evalTrowbridgeReitzPdf_wh(wo, wh, alpha) / (4.0f * dot(wo, wh));
}

inline float evalMicrofacetTransmissionPdf_Beckmann(float3 wo, float3 wi, float2 alpha, float etaA, float etaB)
{ 
    if (isSameHemisphere(wo, wi)) 
		return 0.0f;

	float eta = evalCosTheta(wo) > 0.0f ? (etaB / etaA) : (etaA / etaB);
    float3 wh = normalize(wo + wi * eta);

    // Compute change of variables dwh_dwi for microfacet transmission
    float sqrtDenom = dot(wo, wh) + eta * dot(wi, wh);
    float dwh_dwi = fabs((eta * eta * dot(wi, wh)) / (sqrtDenom * sqrtDenom));
    return evalBeckmannPdf_wh(wo, wh, alpha) * dwh_dwi;
}

inline float evalMicrofacetTransmissionPdf_TrowbridgeReitz(float3 wo, float3 wi, float2 alpha, float etaA, float etaB)
{ 
    if (isSameHemisphere(wo, wi)) 
		return 0.0f;

    float eta = evalCosTheta(wo) > 0.0f ? (etaB / etaA) : (etaA / etaB);
    float3 wh = normalize(wo + wi * eta);

    // Compute change of variables dwh_dwi for microfacet transmission
    float sqrtDenom = dot(wo, wh) + eta * dot(wi, wh);
    float dwh_dwi = fabs((eta * eta * dot(wi, wh)) / (sqrtDenom * sqrtDenom));
    return evalTrowbridgeReitzPdf_wh(wo, wh, alpha) * dwh_dwi;
}

float3 sampleMicrofacetReflection_TorranceSparrow_TrowbridgeReitz(float2 u, float3 R, float2 alpha, float etaI, float etaT, float3 wo, float3* wi, float* pdf)
{
	// Sample microfacet orientation wh and reflected direction wi
	float3 wh = sampleTrowbridgeReitzDistribution_wh(u, wo, alpha);
	*wi = reflect(wo, wh);
	
	if (!isSameHemisphere(wo, *wi))
		return (float3)(0.0f);
	
	*pdf = evalMicrofacetReflectionPdf_TrowbridgeReitz(wo, *wi, wh, alpha);

	//*wi = uniformSampleHemisphere(u);
	//*pdf = uniformHemispherePdf();
	//
	//if (!isSameHemisphere(wo, *wi))
	//	return (float3)(0.0f);

	return evaluateMicrofacetReflection_TorranceSparrow_Dielectric_TrowbridgeReitz(R, alpha, etaI, etaT, wo, *wi);
}

float3 sampleMicrofacetTransmission_TorranceSparrow_TrowbridgeReitz(float2 u, float3 T, TransportMode mode, float2 alpha, float etaA, float etaB, float3 wo, float3* wi, float* pdf)
{
	// Sample microfacet orientation wh and reflected direction wi
	float3 wh = sampleTrowbridgeReitzDistribution_wh(u, wo, alpha);
	float eta = evalCosTheta(wo) > 0.0f ? (etaA / etaB) : (etaB / etaA);
	if (!refract(wo, wh, eta, wi))
		return (float3)(0.0f);

	*pdf = evalMicrofacetTransmissionPdf_TrowbridgeReitz(wo, *wi, alpha, etaA, etaB);

	return evaluateMicrofacetTransmission_TorranceSparrow_TrowbridgeReitz(T, mode, alpha, etaA, etaB, wo, *wi);
}

float3 sampleMicrofacetReflection_TorranceSparrow_Beckmann(float2 u, float3 R, float2 alpha, float etaI, float etaT, float3 wo, float3* wi, float* pdf)
{
	// Sample microfacet normal wh.
	float3 wh = sampleBeckmannDistribution_wh(u, wo, alpha);
	// To get wi a reflection of wo about wh is necessary:
	*wi = reflect(wo, wh);

	if (!isSameHemisphere(wo, *wi))
		return (float3)(0.0f);

	*pdf = evalMicrofacetReflectionPdf_Beckmann(wo, *wi, wh, alpha);

	return evaluateMicrofacetReflection_TorranceSparrow_Dielectric_Beckmann(R, alpha, etaI, etaT, wo, *wi);
}

float3 sampleMicrofacetTransmission_TorranceSparrow_Beckmann(float2 u, float3 T, TransportMode mode, float2 alpha, float etaA, float etaB, float3 wo, float3* wi, float* pdf)
{
	// Sample microfacet orientation wh and reflected direction wi
	float3 wh = sampleBeckmannDistribution_wh(u, wo, alpha);
	float eta = evalCosTheta(wo) > 0.0f ? (etaA / etaB) : (etaB / etaA);
	if (!refract(wo, wh, eta, wi))
		return (float3)(0.0f);

	*pdf = evalMicrofacetTransmissionPdf_Beckmann(wo, *wi, alpha, etaA, etaB);

	return evaluateMicrofacetTransmission_TorranceSparrow_Beckmann(T, mode, alpha, etaA, etaB, wo, *wi);
}

/**
* The uber BSDF may include the following BxDFs:
* SpecularTransmission with transparency (t = 1 - opacity, etaI = etaT = 1), 
* SpecularTransmission with Kt.xyz (etaI = 1, etaT = eta),
* MicrofacetTransmission with FresnelDielectric and TorranceSparrow_TrowbridgeReitz using Kt.xyz (etaI = 1, etaT = eta),
* SpecularReflection with Kr,
* LambertianReflection with Kd,
* MicrofacetReflection with FresnelDielectric and TrowbridgeReitz distribution with Ks.
* A BxDF is only evaluated if the corresponding spectrum isn't black.
*
* This is the evaluation for known woWorld, wiWorld so specular bxdfs can be omitted.
*/
float3 evaluateUberBSDF(float3 Kd, float3 Ks, float4 Kt, float2 roughness, float3 opacity, 
					    float eta, const RTInteraction* si, float3 woWorld, float3 wiWorld, TransportMode mode)
{
	if (!isReflection(woWorld, wiWorld, si))
	{
		bool isPerfectSpecularTransmission = Kt.w < 0.5f;
		if (isPerfectSpecularTransmission)
		{ 
			return (float3)(0.0f);
		}

		float3 wo = dirToShadingSpace(woWorld, si);
		float3 wi = dirToShadingSpace(wiWorld, si);
		float3 kt = Kt.xyz * opacity;
		return evaluateMicrofacetTransmission_TorranceSparrow_TrowbridgeReitz(kt, mode, roughness, 1.0f, eta, wo, wi);
	}

	float3 wo = dirToShadingSpace(woWorld, si);
	float3 wi = dirToShadingSpace(wiWorld, si);
	float3 kd = Kd * opacity;
	float3 ks = Ks * opacity;

	return evaluateMicrofacetReflection_TorranceSparrow_Dielectric_TrowbridgeReitz(ks, roughness, 1.0f, eta, wo, wi) + evaluateLambertianReflection(kd);
}

float evaluateUberBSDF_Pdf(float3 Kd, float3 Ks, float3 Kr, float4 Kt, float2 roughness, float3 opacity, float eta, const RTInteraction* si, float3 woWorld, float3 wiWorld, BxDFType type)
{ 
	float3 wo = dirToShadingSpace(woWorld, si);
	float3 wi = dirToShadingSpace(wiWorld, si);
	if (isNearZero(wo.y))
		return 0.0f;

	float3 t = (float3)(1.0f) - opacity;
	int numBxDFs = 0;
	float3 kd = Kd * opacity;
	float3 ks = Ks * opacity;
	float3 kt = Kt.xyz * opacity;
	float3 kr = Kr * opacity;
	float pdf = 0.0f;

	if (isNotBlack(t)  && BxDFMatchesFlags(BSDF_SPECULAR_TRANSMISSION, type)) ++numBxDFs;
	if (isNotBlack(kr) && BxDFMatchesFlags(BSDF_SPECULAR_REFLECTION, type))   ++numBxDFs;

	if (isNotBlack(kt))
	{
		bool isPerfectSpecularTransmission = Kt.w < 0.5f;
		if (isPerfectSpecularTransmission)
		{ 
			if (BxDFMatchesFlags(BSDF_SPECULAR_TRANSMISSION, type)) ++numBxDFs;
		}
		else
		{ 
			if (BxDFMatchesFlags(BSDF_MICROFACET_TRANSMISSION, type))
			{
				pdf += evalMicrofacetTransmissionPdf_TrowbridgeReitz(wo, wi, roughness, 1.0f, eta);
				++numBxDFs;
			}
		}
	}

	if (isNotBlack(kd) && BxDFMatchesFlags(BSDF_LAMBERTIAN_REFLECTION, type))
	{
		pdf += evaluateLambertianReflectionPdf(wo, wi);
		++numBxDFs;
	}

	if (isNotBlack(ks) && BxDFMatchesFlags(BSDF_MICROFACET_REFLECTION, type))
	{
		pdf += evalMicrofacetReflectionPdf_TrowbridgeReitz(wo, wi, normalize(wo + wi), roughness);
		++numBxDFs;
	}

	if (numBxDFs > 1)
		pdf /= numBxDFs;

	return pdf;
}

/**
* The uber BSDF may include the following BxDFs:
* SpecularTransmission with transparency (t = 1 - opacity, etaI = etaT = 1), 
* SpecularTransmission with Kt.xyz (etaI = 1, etaT = eta),
* MicrofacetTransmission with FresnelDielectric and TorranceSparrow_TrowbridgeReitz using Kt.xyz (etaI = 1, etaT = eta),
* SpecularReflection with Kr,
* LambertianReflection with Kd,
* MicrofacetReflection with FresnelDielectric and TrowbridgeReitz distribution with Ks.
* A BxDF is only evaluated if the corresponding spectrum isn't black.
*/
float3 sampleUberBSDF(float3 Kd, float3 Ks, float3 Kr, float4 Kt, float2 roughness, float3 opacity, 
					  float eta, const RTInteraction* si, float2 u, TransportMode mode, BxDFType type, 
					  float3 woWorld, float3* wiWorld, float* pdf, int* numNonDeltaTypes, BxDFType* sampledType)
{
	// It is important to distinguish between specular and non-specular (glossy, diffuse) reflection and transmission.
	// 1. If a specular bxdf is sampled the pdf is simply 1 / numberOfTypeMatchingBxDFs and the returned value is the result of the sampled bxdf.
	// 2. If a non-specular bxdf is sampled the pdf is the sum of pdfs of matching bxdfs divided by the number of matching bxdfs.
	//    Since specular bxdfs are described by a delta distribution their pdf must be zero in this case.

	float3 t = (float3)(1.0f) - opacity;
	int numBxDFs = 0;
	float3 kd = Kd * opacity;
	float3 ks = Ks * opacity;
	float3 kt = Kt.xyz * opacity;
	float3 kr = Kr * opacity;
	float3 wo = dirToShadingSpace(woWorld, si);
	float3 wi;
	bool isPerfectSpecularTransmission = Kt.w < 0.5f;
	*sampledType = BSDF_NONE;
	*numNonDeltaTypes = 0;

	// 1. A random BxDF must be chosen to sample. It is thus necessary to first
	// determine how many matching BxDFs there are.
	if (isNotBlack(t) && BxDFMatchesFlags(BSDF_SPECULAR_TRANSMISSION, type))
	{ 
		++numBxDFs;
	}
	if (isNotBlack(kd) && BxDFMatchesFlags(BSDF_LAMBERTIAN_REFLECTION, type))
	{
		(*numNonDeltaTypes)++;
		++numBxDFs;
	}

	if (isNotBlack(ks) && BxDFMatchesFlags(BSDF_MICROFACET_REFLECTION, type))
	{
		(*numNonDeltaTypes)++;
		++numBxDFs;
	}
	if (isNotBlack(kr) && BxDFMatchesFlags(BSDF_SPECULAR_REFLECTION, type))
	{ 
		++numBxDFs;
	}

	if (isNotBlack(kt))
	{ 
		if (isPerfectSpecularTransmission)
		{ 
			if (BxDFMatchesFlags(BSDF_SPECULAR_TRANSMISSION, type)) ++numBxDFs;
		}
		else
		{ 
			if (BxDFMatchesFlags(BSDF_MICROFACET_TRANSMISSION, type))
			{ 
				(*numNonDeltaTypes)++;
				++numBxDFs;
			}
		}
	}

	// It is possible that the number of matching BxDFs is 0.
	if (numBxDFs == 0)
		return (float3)(0.0f, 0.0f, 0.0f);

	// 2. Now a random BxDF can be chosen.
	int chosenBxDFIdx = min((int)floor(u.x * numBxDFs), numBxDFs - 1);

	// Since u.x was used to choose a BxDF the value must be remapped for a correct sampling distribution.
	u.x = u.x * numBxDFs - chosenBxDFIdx;

	float3 f = (float3)(0.0f);
	*pdf = 0.0f;
	bool isSamplingSpecular = false;

	// 3. Now sample the chosen bxdf, compute average pdf 

	// Handle opacity
	if (isNotBlack(t) && BxDFMatchesFlags(BSDF_SPECULAR_TRANSMISSION, type))
	{ 
		if (chosenBxDFIdx-- == 0)
		{ 
			f += sampleSpecularTransmission(t, 1.0f, 1.0f, mode, wo, &wi, pdf);
			*sampledType |= BSDF_SPECULAR_TRANSMISSION;
			isSamplingSpecular = true;
		}
	}

	// Handle perfect specular transmission
	if (isPerfectSpecularTransmission && isNotBlack(kt) && BxDFMatchesFlags(BSDF_SPECULAR_TRANSMISSION, type))
	{ 
		if (chosenBxDFIdx-- == 0)
		{ 
			f += sampleSpecularTransmission(kt, 1.0f, eta, mode, wo, &wi, pdf);
			*sampledType |= BSDF_SPECULAR_TRANSMISSION;
			isSamplingSpecular = true;
		}
	}

	// Handle perfect specular reflection
	if (isNotBlack(kr) && BxDFMatchesFlags(BSDF_SPECULAR_REFLECTION, type))
	{ 
		if (chosenBxDFIdx-- == 0)
		{ 
			f += sampleSpecularReflection_Dielectric(kr, 1.0f, eta, wo, &wi, pdf);
			*sampledType |= BSDF_SPECULAR_REFLECTION;
			isSamplingSpecular = true;
		}
	}

	// Handle microfacet transmission
	// Note: There is no evaluate() call for microfacet transmission because wo, wi are guaranteed to be in the same hemisphere (in this particular material) if 
	// microfacet transmission isn't chosen to be sampled.
	// Microfacert transmission evaluation is only applicable if wo, wi are in different hemispheres.
	if (isNotBlack(kt) && BxDFMatchesFlags(BSDF_MICROFACET_TRANSMISSION, type))
	{ 
		if (chosenBxDFIdx-- == 0)
		{ 
			f += sampleMicrofacetTransmission_TorranceSparrow_TrowbridgeReitz(u, kt, mode, roughness, 1.0f, eta, wo, &wi, pdf);
			*sampledType |= BSDF_MICROFACET_TRANSMISSION;
		}
	}

	// Handle lambertian reflection
	bool isLambertianEvaluationRequested = false;
	if (isNotBlack(kd) && BxDFMatchesFlags(BSDF_LAMBERTIAN_REFLECTION, type))
	{ 
		if (chosenBxDFIdx-- == 0)
		{ 
			f += sampleLambertianReflection(kd, u, wo, &wi, pdf);
			*sampledType |= BSDF_LAMBERTIAN_REFLECTION;
		}
		else if (!isSamplingSpecular && isSameHemisphere(wi, wo))
		{
			isLambertianEvaluationRequested = true;
		}
	}

	// Handle microfacet reflection
	if (isNotBlack(ks) && BxDFMatchesFlags(BSDF_MICROFACET_REFLECTION, type))
	{ 
		if (chosenBxDFIdx-- == 0)
		{ 
			f += sampleMicrofacetReflection_TorranceSparrow_TrowbridgeReitz(u, ks, roughness, 1.0f, eta, wo, &wi, pdf);
			*sampledType |= BSDF_MICROFACET_REFLECTION;
		}
		else if (!isSamplingSpecular && isSameHemisphere(wi, wo))
		{
			f += evaluateMicrofacetReflection_TorranceSparrow_Dielectric_TrowbridgeReitz(ks, roughness, 1.0f, eta, wo, wi);
			*pdf += evalMicrofacetReflectionPdf_TrowbridgeReitz(wo, wi, normalize(wo + wi), roughness);
		}
	}

	if (isLambertianEvaluationRequested)
	{ 
		f += evaluateLambertianReflection(kd);
		*pdf += evaluateLambertianReflectionPdf(wo, wi);
	}

	*pdf /= numBxDFs;
	*wiWorld = dirFromShadingToWorld(wi, si);

	return f;
}

#endif