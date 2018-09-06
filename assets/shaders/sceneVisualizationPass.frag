#version 430
#extension GL_ARB_shading_language_include : enable
#include "/voxelConeTracing/conversion.glsl"
#include "/BRDF.glsl"

// Attributes
in Vertex
{
    vec2 texCoords;
} In;

// Out
layout(location = 0) out vec4 out_color;

uniform sampler2D u_diffuseTexture;
uniform sampler2D u_normalMap;
uniform sampler2D u_specularMap;
uniform sampler2D u_emissionMap;
uniform sampler2D u_depthTexture;
uniform sampler2D u_geometryNormalMap;
uniform sampler2D u_uvMap;

#define DISPLAY_DIFFUSE_TEXTURE 1
#define DISPLAY_NORMAL_MAP 2
#define DISPLAY_SPECULAR_MAP 3
#define DISPLAY_EMISSION_MAP 4
#define DISPLAY_DEPTH_TEXTURE 5
#define DISPLAY_GEOMETRY_NORMAL_MAP 6
#define DISPLAY_UV_MAP 7

uniform vec3 u_eyePos;
uniform vec3 u_cameraDir;
uniform mat4 u_viewProjInv;
uniform int u_displayMode;
uniform int u_BRDFMode;

vec3 worldPosFromDepth(float depth)
{
    vec4 p = vec4(In.texCoords, depth, 1.0);
    p.xyz = p.xyz * 2.0 - 1.0;
    p = u_viewProjInv * p;
    return p.xyz / p.w;
}

void main() 
{
    float depth = texture2D(u_depthTexture, In.texCoords).r;
    if (depth == 1.0)
        discard;
        
    vec3 diffuse = texture(u_diffuseTexture, In.texCoords).rgb;
    vec3 normal = unpackNormal(texture(u_normalMap, In.texCoords).rgb);
    vec3 emission = texture(u_emissionMap, In.texCoords).rgb;
    bool hasEmission = any(greaterThan(emission, vec3(0.0)));  
    vec3 posW = worldPosFromDepth(depth);
    vec3 view = normalize(u_eyePos - posW);
    vec4 specColor = texture(u_specularMap, In.texCoords);
	vec3 geometryNormal = texture(u_geometryNormalMap, In.texCoords).rgb;
	vec2 uv = texture(u_uvMap, In.texCoords).rg;

	if (u_displayMode == DISPLAY_DIFFUSE_TEXTURE)
	{
		out_color.rgb = diffuse;
	}
	else if (u_displayMode == DISPLAY_NORMAL_MAP)
	{
		out_color.rgb = normal;
	}
	else if (u_displayMode == DISPLAY_SPECULAR_MAP)
	{
		out_color = specColor;
	}
	else if (u_displayMode == DISPLAY_EMISSION_MAP)
	{
		out_color.rgb = emission;
	}
	else if (u_displayMode == DISPLAY_DEPTH_TEXTURE)
	{
		out_color.rgb = depth.rrr * depth.rrr * depth.rrr * depth.rrr;
	}
	else if (u_displayMode == DISPLAY_GEOMETRY_NORMAL_MAP)
	{
		out_color.rgb = geometryNormal;
	}
	else if (u_displayMode == DISPLAY_UV_MAP)
	{
		out_color.rg = uv;
		out_color.b = 0.0f;
	}
	else
	{
		// Lit
		vec3 lightDir = -view; 
		vec3 halfway = normalize(view - lightDir);
		vec3 lightColor = vec3(1.0);
		float shininess = unpackShininess(specColor.a);
		if (u_BRDFMode == BLINN_PHONG_MODE_IDX)
        {
            out_color.rgb = blinnPhongBRDF(lightColor, diffuse, lightColor, specColor.rgb, normal, -lightDir, halfway, shininess);
        } 
		else if (u_BRDFMode == COOK_TORRANCE_MODE_IDX)
        {
			float roughness = shininessToRoughness(shininess);
			float nDotL = dot(-lightDir, normal);
            vec3 cook = cookTorranceBRDF(-lightDir, normal, view, halfway, roughness, specColor.rgb * 0.5);
            out_color.rgb = cook * lightColor * specColor.rgb + lightColor * diffuse * nDotL;
        }
	}
}
