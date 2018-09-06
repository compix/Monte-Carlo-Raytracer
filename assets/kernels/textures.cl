#ifndef TEXTURES_CL
#define TEXTURES_CL

/**
* Using built in OpenCL images is not practical (with OpenCL 1.2) because arrays of images (image2d_t[]) aren't supported.
* A possible workaround is it use image arrays (image2d_array_t), however the size and format must be identical for all images in the array.
*
* textures.cl provides custom texture implementations based on global uchar buffers passed in to a kernel.
* Note that both solutions (texture arrays and buffers) require texture memory duplication if OpenCL and OpenGL are combined
* to implement hybrid rendering - rasterization and raytracing - on the same GPU.
*
* Texture descriptions are used to specify size, format, wrapping method and memory location in the texture buffer.
* Supported formats and wrapping methods are defined in the enums below.
*
* Texture data is 4 byte aligned. Formats R8, RG8, RGB8, RGBA8 are all treated as RGBA8 and thus have 4 color channels and a texel stride of 4 bytes.
* If a texture with format R8 is read then (r, 0, 0, 1) is returned, where r is the value of the texture converted to floating point value in [0,1].
*/

#include <kernel_data.h>

/**
* Computes the stride of one texel in bytes based on texture format.
*/
inline int computeTexelStride(ushort format)
{ 
	return 4;
}

// Read texture with nearest filtering
float4 readTexture2Df_nearest(const TextureDesc2D* tex, __global const uchar* texData, float2 uv)
{
	switch (tex->wrap)
	{
		case TEX_WRAP_REPEAT:
		{
			uv -= floor(uv);
			break;
		}
		case TEX_WRAP_MIRRORED_REPEAT:
		{
			if (uv.x > 1.0f || uv.x < 0.0f)
				uv.x = 1.0f - (uv.x - floor(uv.x));
			if (uv.y > 1.0f || uv.y < 0.0f)
				uv.y = 1.0f - (uv.y - floor(uv.y));
			break;
		}
		case TEX_WRAP_CLAMP_TO_EDGE:
		{
			uv = (float2)(clamp(uv.x, 0.0f, 1.0f), clamp(uv.y, 0.0f, 1.0f));
			break;
		}
		case TEX_WRAP_CLAMP_TO_BORDER:
		{ 
			if (uv.x > 1.0f || uv.x < 0.0f || uv.y > 1.0f || uv.y < 0.0f)
				return TEX_BORDER_COLOR;
		}
	}

	int x = clamp((int)round(uv.x * tex->width), 0, tex->width - 1);
	int y = clamp((int)round(uv.y * tex->height), 0, tex->height - 1);

	__global uchar4 const* texD = (__global const uchar4*)(texData + tex->memOffset);

	return convert_float4(*(texD + x + y * tex->width)) * (1.0f / 255.0f);
}

/**
* Read texture with linear interpolation filtering.
*/
float4 readTexture2Df_linear(const TextureDesc2D* tex, __global const uchar* texData, float2 uv)
{
	// Sample at the center of four pixels
	uv.x -= 1.0f / tex->width * 0.5f;
	uv.y -= 1.0f / tex->height * 0.5f;

	switch (tex->wrap)
	{
		case TEX_WRAP_REPEAT:
		{
			uv -= floor(uv);
			break;
		}
		case TEX_WRAP_MIRRORED_REPEAT:
		{
			if (uv.x > 1.0f || uv.x < 0.0f)
				uv.x = 1.0f - (uv.x - floor(uv.x));
			if (uv.y > 1.0f || uv.y < 0.0f)
				uv.y = 1.0f - (uv.y - floor(uv.y));
			break;
		}
		case TEX_WRAP_CLAMP_TO_EDGE:
		{
			uv = (float2)(clamp(uv.x, 0.0f, 1.0f), clamp(uv.y, 0.0f, 1.0f));
			break;
		}
		case TEX_WRAP_CLAMP_TO_BORDER:
		{ 
			if (uv.x > 1.0f || uv.x < 0.0f || uv.y > 1.0f || uv.y < 0.0f)
				return TEX_BORDER_COLOR;
		}
	}

	int x0 = ((int)floor(uv.x * tex->width)) % tex->width;//clamp((int)floor(uv.x * tex->width), 0, tex->width - 1);
	int y0 = ((int)floor(uv.y * tex->height)) % tex->height;//clamp((int)floor(uv.y * tex->height), 0, tex->height - 1);
	int x1 = (x0 + 1) % tex->width;//clamp(x0 + 1, 0, tex->width - 1);
	int y1 = (y0 + 1) % tex->height;//clamp(y0 + 1, 0, tex->height - 1);

	x0 = clamp(x0, 0, tex->width - 1);
	y0 = clamp(y0, 0, tex->height - 1);
	x1 = clamp(x1, 0, tex->width - 1);
	y1 = clamp(y1, 0, tex->height - 1);

	// Interpolation values
	float2 t = (float2)(uv.x * tex->width  - floor(uv.x * tex->width), 
						uv.y * tex->height - floor(uv.y * tex->height));

	__global uchar4 const* texD = (__global const uchar4*)(texData + tex->memOffset);
		
	float4 v00 = convert_float4(*(texD + x0 + y0 * tex->width));
	float4 v10 = convert_float4(*(texD + x1 + y0 * tex->width));
	float4 v01 = convert_float4(*(texD + x0 + y1 * tex->width));
	float4 v11 = convert_float4(*(texD + x1 + y1 * tex->width));
			
	return mix(mix(v00, v10, t.x), mix(v01, v11, t.x), t.y) * (1.0f / 255.0f);
}

int computeMipLevelByteOffset(const TextureDesc2D* tex, int texelStride, int level)
{ 
	ushort w = tex->width;
	ushort h = tex->height;
	int offset = 0;

	for (int i = 0; i < level; ++i)
	{ 
		offset += w * h * texelStride;
		w = max(w / 2, 1);
		h = max(h / 2, 1);
	}

	return offset;
}

/**
* Read texture with mip map filtering.
* TODO: Optimize by precomputing byte offsets, use uint16 to store memoffsets.
* Note: There is a lot of room for optimization - it will, however, make the code less readable.
*/
float4 readTexture2Df_lod(const TextureDesc2D* tex, __global const uchar* texData, float2 uv, float lod)
{ 
	if (lod < 1e-8f || tex->numMipLevels < 2)
		return readTexture2Df_linear(tex, texData, uv);
	
	int texelStride = computeTexelStride(tex->format);
	
	if (lod >= (tex->numMipLevels - 1))
	{
		TextureDesc2D highestLevelTexDesc = *tex;
		highestLevelTexDesc.width = 1;
		highestLevelTexDesc.height = 1;
		highestLevelTexDesc.memOffset += computeMipLevelByteOffset(tex, texelStride, tex->numMipLevels - 1);
		return readTexture2Df_linear(&highestLevelTexDesc, texData, uv);
	}

	int lowerLevel = floor(lod);
	
	ushort w = tex->width;
	ushort h = tex->height;
	int byteOffsetLowerLevel = 0;

	for (int i = 0; i < lowerLevel; ++i)
	{ 
		byteOffsetLowerLevel += w * h * texelStride;
		w = max(w / 2, 1);
		h = max(h / 2, 1);
	}

	TextureDesc2D texDescLowerLevel = *tex;
	texDescLowerLevel.width = w;
	texDescLowerLevel.height = h;
	texDescLowerLevel.memOffset += byteOffsetLowerLevel;

	int byteOffsetUpperLevel = byteOffsetLowerLevel + w * h * texelStride;
	w = max(w / 2, 1);
	h = max(h / 2, 1);

	TextureDesc2D texDescUpperLevel = *tex;

	texDescUpperLevel.width = w;
	texDescUpperLevel.height = h;
	texDescUpperLevel.memOffset += byteOffsetUpperLevel;

	float4 v0 = readTexture2Df_linear(&texDescLowerLevel, texData, uv);
	float4 v1 = readTexture2Df_linear(&texDescUpperLevel, texData, uv);

	return mix(v0, v1, lod - lowerLevel);
}

inline float computeMipmapLOD(const TextureDesc2D* texDesc, float2 duvdx, float2 duvdy)
{ 
	float sampleWidth = fmax(fmax(fabs(duvdx.x), fabs(duvdx.y)), fmax(fabs(duvdy.x), fabs(duvdy.y)));
	return texDesc->numMipLevels - 1.0f + log2(fmax(sampleWidth, 1e-8f));
}

inline float4 readTexture2Df(int texId, __global const TextureDesc2D* textures, __global const uchar* texData, const RTInteraction* si)
{
	TextureDesc2D desc = textures[texId];
	//diff_col = readTexture2Df_lod(&diffTexDesc, scene_texData2D, si.uv, computeMipmapLOD(&diffTexDesc, si.duvdx, si.duvdy));
	return readTexture2Df_linear(&desc, texData, si->uv);
}

inline float4 readTexture2Df_ifValid(int texId, __global const TextureDesc2D* textures, __global const uchar* texData, const RTInteraction* si, float4 fallbackColor)
{
	if (texId != RT_INVALID_ID)
		return readTexture2Df(texId, textures, texData, si);

	return fallbackColor;
}

inline float3 readTexture2Df3_ifValid(int texId, __global const TextureDesc2D* textures, __global const uchar* texData, const RTInteraction* si, float3 fallbackColor)
{
	if (texId != RT_INVALID_ID)
		return readTexture2Df(texId, textures, texData, si).xyz;

	return fallbackColor;
}

inline float2 readTexture2Df2_ifValid(int texId, __global const TextureDesc2D* textures, __global const uchar* texData, const RTInteraction* si, float2 fallbackColor)
{
	if (texId != RT_INVALID_ID)
		return readTexture2Df(texId, textures, texData, si).xy;

	return fallbackColor;
}

inline float readTexture2Df1_ifValid(int texId, __global const TextureDesc2D* textures, __global const uchar* texData, const RTInteraction* si, float fallbackColor)
{
	if (texId != RT_INVALID_ID)
		return readTexture2Df(texId, textures, texData, si).x;

	return fallbackColor;
}

__kernel void testTexture2D(__global const TextureDesc2D* textures, __global const uchar* texData, uint screenWidth, uint screenHeight, write_only image2d_t image)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

	if (gid.x >= screenWidth || gid.y >= screenHeight)
		return;

	float2 uv = (float2)(gid.x, gid.y);
	float2 ts = (float2)(textures->width, textures->height);
	float2 ss = (float2)(screenWidth, screenHeight);
	uv.x /= ss.x;
	uv.y /= ss.y;
	TextureDesc2D texDesc = *textures;
	float4 val = readTexture2Df_linear(&texDesc, texData, uv);
	write_imagef(image, gid, val * val.w);
}

__kernel void testTexture2D_nearest(__global const TextureDesc2D* textures, __global const uchar* texData, uint screenWidth, uint screenHeight, write_only image2d_t image)
{ 
	int2 gid = (int2)(get_global_id(0), get_global_id(1));

	if (gid.x >= screenWidth || gid.y >= screenHeight)
		return;

	float2 uv = (float2)(gid.x, gid.y);
	float2 ts = (float2)(textures->width, textures->height);
	float2 ss = (float2)(screenWidth, screenHeight);
	uv.x /= ss.x;
	uv.y /= ss.y;
	TextureDesc2D texDesc = *textures;
	float4 val = readTexture2Df_nearest(&texDesc, texData, uv);
	write_imagef(image, gid, val * val.w);
}



#endif