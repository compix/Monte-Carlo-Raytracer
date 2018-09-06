/**
* This header is used for C++ and OpenCL to define data types without
* duplication and to speed up the workflow for data type creation and manipulation.
*/

#ifndef DATA_H
#define DATA_H

#define RT_INVALID_ID -1
#define RT_TRACE_OFFSET 0.00001f // Lower values cause errors (e.g. black pixels for occlusion queries)
#define RT_ENABLE_SHADOWS
#define RT_MAX_TRACE_DISTANCE 1000.0f
#define RT_MAX_ALLOWED_RADIANCE 1000

#ifdef __cplusplus
#include <radeon_rays.h>

typedef RadeonRays::matrix rt_mat4;
typedef uint32_t rt_uint32;
typedef RadeonRays::float2 rt_float2;
typedef RadeonRays::float3 rt_float3;
typedef RadeonRays::float4 rt_float4;

#else // Otherwise assuming that it's OpenCL

// OpenCL has no built in type for matrices
#include "matrix.cl"

typedef uint rt_uint32;
typedef mat4 rt_mat4;
typedef float2 rt_float2;
typedef float3 rt_float3;
typedef float4 rt_float4;
#endif

typedef struct _RTShape
{
#ifdef __cplusplus
	_RTShape() : materialId(RT_INVALID_ID), lightID(RT_INVALID_ID), area(1.0f) {}
#endif

	rt_mat4 toWorldTransform;
	rt_mat4 toWorldInverseTranspose;
	rt_uint32 startIdx;
	rt_uint32 startVertex;
	rt_uint32 numTriangles;
	int materialId;
	int lightID;
	float area;

	int pad[2];
} RTShape;

enum RTFilterType
{
	RT_BOX_FILTER = 0,
	RT_TRIANGLE_FILTER,
	RT_GAUSSIAN_FILTER,
	RT_MITCHELL_FILTER,
	RT_LANCZOS_SINC_FILTER
};

typedef struct _RTFilterProperties
{
	int filterType;

	rt_float2 radius;
	float mitchellB;
	float mitchellC;

	float lanczosSincTau;

	float gaussianAlpha;
	float gaussianExpX;
	float gaussianExpY;

	rt_float2 pixelOffset;

	float pad;
} RTFilterProperties;

enum RTMaterialType
{
	RT_UBER_MATERIAL
};

typedef struct _RTMaterial
{
#ifdef __cplusplus
	_RTMaterial() : uber_normalMapId(RT_INVALID_ID), uber_diffuseTexId(RT_INVALID_ID), uber_glossyTexId(RT_INVALID_ID),
		uber_specReflectionTexId(RT_INVALID_ID), uber_transmissionTexId(RT_INVALID_ID), uber_opacityTexId(RT_INVALID_ID), 
		uber_roughnessTexId(RT_INVALID_ID), uber_iorTexId(RT_INVALID_ID),
		uber_kd(0.25f,0.25f,0.25f), uber_ks(0.25f,0.25f,0.25f), uber_kr(0.0f), uber_kt(0.0f), 
		uber_roughness(0.1f,0.1f), uber_eta(1.5f), type(RT_UBER_MATERIAL), uber_opacity(1.0f,1.0f,1.0f) {}
#endif
	rt_float3 uber_kd; // diffuse, lambertian reflection
	rt_float3 uber_ks; // glossy specular reflection
	rt_float3 uber_kr; // perfect specular reflection
	rt_float4 uber_kt; // glossy if uber_kt.w >= 0.5 otherwise perfect specular transmission
	rt_float3 uber_opacity;
	rt_float2 uber_roughness;
	float uber_eta;

	int type;
	int uber_normalMapId;
	int uber_diffuseTexId; // Kd
	int uber_glossyTexId; // Ks
	int uber_specReflectionTexId; // Kr
	int uber_transmissionTexId; // Kt
	int uber_opacityTexId;
	int uber_roughnessTexId;
	int uber_iorTexId; // Index of refraction
} RTMaterial;

enum RTLightType
{
	RT_DIRECTIONAL_LIGHT,
	RT_POINT_LIGHT,
	RT_DISK_AREA_LIGHT,
	RT_TRIANGLE_MESH_AREA_LIGHT
};

enum RTLightFlags
{
	RT_LIGHT_FLAG_DELTA_POSITION = (1 << 1),
	RT_LIGHT_FLAG_DELTA_DIRECTION = (1 << 2),
	RT_LIGHT_FLAG_AREA = (1 << 3),
	RT_LIGHT_FLAG_INFINITE = (1 << 4),

	RT_LIGHT_FLAG_DELTA_LIGHT = (RT_LIGHT_FLAG_DELTA_POSITION | RT_LIGHT_FLAG_DELTA_DIRECTION)
};

/**
* Additional data that may be needed:
* - toWorldTransform, fromWorldTransform
*/
typedef struct _RTLight
{
	// Direction in world space, may be interpreted as a normal
	rt_float3 d;

	// World position
	rt_float3 p;
	rt_float3 intensity;
	float radius;
	float area;
	float choicePdf;
	int shapeId;
	int type;
	int flags;
	int pad[2];
} RTLight;

typedef struct _RTThroughput
{
	rt_float3 throughput;
	int prevBsdfFlags;
	int ignoreOcclusion;
	int pad[2];
} RTThroughput;

typedef struct _RTInteraction
{
	// Outgoing direction vector in world space.
	// In rasterization this would be the view vector from shaded position to the eye.
	rt_float3 wo;

	// Position in world space
	rt_float3 p;

	// Surface and texture coordinates
	rt_float2 uv;

	float traceErrorOffset;

	int shapeIdx;

	// Geometry normal in world space
	rt_float3 gn;

	// Shading normal in world space
	rt_float3 sn;

	// Position derivatives in world space
	rt_float3 dpdu;
	rt_float3 dpdv;

	// Shading position derivatives in world space based on texture coordinates
	rt_float3 sdpdu;
	rt_float3 sdpdv;

	rt_float3 dpdx;
	rt_float3 dpdy;
	rt_float2 duvdx;
	rt_float2 duvdy;

	// Medium info
	//int mediumInsideId; // Inside the shape
	//int mediumOutsideId; // Outside of the shape
} RTInteraction;

enum RTBDPTVertexType
{
	RT_BDPT_CAMERA_VERTEX,
	RT_BDPT_LIGHT_VERTEX,
	RT_BDPT_SURFACE_VERTEX
};

enum RTBDPTVertexFlag
{
	// A vertex is not connectible if it's of type light and has a delta direction or
	// if it is of type surface and has only delta components otherwise it's connectible.
	RT_BDPT_VERTEX_FLAG_CONNECTIBLE = 1,
	RT_BDPT_VERTEX_FLAG_DELTA_LIGHT = (1 << 1),
	// Only used by surface interactions, indicates if a delta bxdf was sampled
	RT_BDPT_VERTEX_FLAG_DELTA = (1 << 2),
	RT_BDPT_VERTEX_FLAG_INFINITE_LIGHT = (1 << 3)
};

typedef struct _RTBDPTVertex
{
	/**
	* Product of BSDF results and cos terms divided by pdfs.
	* The light subpath includes emitted radiance divided by position and direction pdfs of the emission.
	* Also referred to as beta in PBR book.
	*/
	rt_float3 throughput;
	RTInteraction interaction;
	int type;
	int flags;

	int lightIdx;
	int materialIdx;

	// Forward density computed for importance-carrying paths (from light source to the camera).
	float pdfFwd;
	// Reversed density computed for radiance transport (from camera to light source).
	float pdfRev;

	float pdfPos;

	int radianceBufferIdx;

} RTBDPTVertex;

typedef struct _RTPinholeCamera
{
	rt_mat4 worldToClip;
	
	rt_float3 r00;
	rt_float3 r10;
	rt_float3 r11;
	rt_float3 r01;

	rt_float3 pos;
	rt_float3 direction;

	rt_uint32 width;
	rt_uint32 height;

	float area;

	int padding;
} RTPinholeCamera;

typedef struct
{
	rt_float3 xOrigin;
	rt_float3 yOrigin;
	rt_float3 xDirection;
	rt_float3 yDirection;
} RTRayDifferentials;

// Scene info
#ifdef __cplusplus
#include "application/PathTracer/Raytracing/textures/RTTextures.h"

struct RTDeviceScene
{
	CLWBuffer<RTShape> shapes;
	CLWBuffer<uint32_t> indices;
	CLWBuffer<RadeonRays::float3> positions;
	CLWBuffer<RadeonRays::float2> uvs;
	CLWBuffer<RadeonRays::float3> normals;
	CLWBuffer<RadeonRays::float3> tangents;
	CLWBuffer<RadeonRays::float3> binormals;
	CLWBuffer<RadeonRays::float3> colors;
	CLWBuffer<RTTextureDesc2D> textures;
	CLWBuffer<unsigned char> textureData;
	CLWBuffer<uint32_t> sobolMatrices;
	CLWBuffer<RTLight> lights;
	CLWBuffer<RTMaterial> materials;
	CLWBuffer<RTPinholeCamera> camera;
};

#else // Otherwise assuming that it's OpenCL

/******************** Texture Data **********************/
enum TextureFormat
{
	TEX_FORMAT_R8,
	TEX_FORMAT_RG8,
	TEX_FORMAT_RGB8,
	TEX_FORMAT_RGBA8
};

#define TEX_BORDER_COLOR (float4)(0.0f, 0.0f, 0.0f, 0.0f);

enum TextureWrapping
{
	TEX_WRAP_REPEAT,
	TEX_WRAP_MIRRORED_REPEAT,
	TEX_WRAP_CLAMP_TO_EDGE,
	TEX_WRAP_CLAMP_TO_BORDER
};

typedef struct
{
	ushort width;
	ushort height;
	ushort numMipLevels;
	ushort format;
	ushort wrap;
	ushort pad;
	uint memOffset; // Byte offset
} TextureDesc2D;

#define TRACE_PARAMS __global RTRay* trace_shadowRays,\
				     __global RTRay* trace_rays,\
					 __global RTIntersection* trace_isects

#define INTEGRATOR_PARAMS int integrator_frameNum,\
					      int integrator_maxDepth,\
						  int integrator_bounceIdx,\
						  __global float4* integrator_radianceBuffer,\
						  __global RTThroughput* integrator_throughputBuffer

#define SCENE_PARAMS __global const RTShape* restrict scene_shapes,\
					 __global const unsigned int* restrict scene_indices,\
				     __global const float3* restrict scene_positions, \
				     __global const float2* restrict scene_uvs, \
				     __global const float3* restrict scene_normals, \
				     __global const float3* restrict scene_tangents, \
				     __global const float3* restrict scene_binormals, \
				     __global const float3* restrict scene_colors, \
					 __global const TextureDesc2D* scene_textures2D,\
					 __global const uchar* scene_texData2D,\
					 __global const uint* scene_sobolMatrices,\
					 __global const RTLight* scene_lights,\
					 int scene_numLights,\
					 __global const RTMaterial* scene_materials,\
					 __global const RTPinholeCamera* scene_camera

#define IMAGE_PARAMS int image_width,\
				     int image_height
					 

typedef struct _Scene
{
	__global const RTShape* restrict shapes;
	__global const unsigned int* restrict indices;
	__global const float3* restrict positions;
	__global const float2* restrict uvs;
	__global const float3* restrict normals;
	__global const float3* restrict tangents;
	__global const float3* restrict binormals;
	__global const float3* restrict colors;
	__global const TextureDesc2D* textures2D;
	__global const uchar* texData2D;
	__global const uint* sobolMatrices;
	__global const RTLight* lights;
	__global const RTMaterial* materials;
	__global const RTPinholeCamera* camera;
	int numLights;
} Scene;

#define MAKE_SCENE(scene) 	Scene scene;\
	scene.shapes = scene_shapes;\
	scene.indices = scene_indices;\
	scene.positions = scene_positions;\
	scene.uvs = scene_uvs;\
	scene.normals = scene_normals;\
	scene.tangents = scene_tangents;\
	scene.binormals = scene_binormals;\
	scene.colors = scene_colors;\
	scene.textures2D = scene_textures2D;\
	scene.texData2D = scene_texData2D;\
	scene.sobolMatrices = scene_sobolMatrices;\
	scene.lights = scene_lights;\
	scene.numLights = scene_numLights;\
	scene.materials = scene_materials;\
	scene.camera = scene_camera;

typedef struct
{
	// id of a shape
	int shapeid;
	// Primitive index
	int primid;
	// Padding elements
	int2 padding;

	// uv - hit barycentrics, w - ray distance
	float4 uvwt;
} RTIntersection;

typedef struct
{
	/// xyz - origin, w - max range
	float4 o;
	/// xyz - direction, w - time
	float4 d;
	/// x - ray mask, y - activity flag
	int2 extra;
	int doBackfaceCulling;
	int padding;
} RTRay;

inline bool isRayActive(__global const RTRay* ray)
{
	return ray->extra.y != 0;
}

inline bool isRayInactive(__global const RTRay* ray)
{
	return ray->extra.y == 0;
}

inline void setRayInactive(__global RTRay* ray)
{
	ray->extra.y = 0;
}

inline void setRay(__global RTRay* ray, float3 p, float maxRange, float3 d)
{
	ray->o.xyz = p;
	ray->o.w = maxRange;
	ray->d.xyz = d;
	ray->extra = (int2)(0xFFFFFFFF);
}

inline bool isInfiniteLight(__global RTLight* light)
{
	return (light->flags & RT_LIGHT_FLAG_DELTA_DIRECTION) != 0;
}

inline bool isVertexDelta(__global const RTBDPTVertex* vertex)
{
	return (vertex->flags & RT_BDPT_VERTEX_FLAG_DELTA) != 0;
}

inline bool isVertexConnectible(__global const RTBDPTVertex* vertex)
{
	return (vertex->flags & RT_BDPT_VERTEX_FLAG_CONNECTIBLE) != 0;
}

inline bool isVertexLight(__global const RTBDPTVertex* vertex)
{
	return vertex->type == RT_BDPT_LIGHT_VERTEX || vertex->lightIdx != RT_INVALID_ID;
}

inline bool isVertexDeltaLight(__global const RTBDPTVertex* vertex)
{
	return (vertex->flags & RT_BDPT_VERTEX_FLAG_DELTA_LIGHT) != 0;
}

inline bool isVertexInfiniteLight(__global const RTBDPTVertex* vertex)
{
	return (vertex->flags & RT_BDPT_VERTEX_FLAG_INFINITE_LIGHT) != 0;
}

inline bool isVertexCamera(__global const RTBDPTVertex* vertex)
{
	return vertex->type == RT_BDPT_CAMERA_VERTEX;
}

#endif // End OpenCL code region

#endif // DATA_H