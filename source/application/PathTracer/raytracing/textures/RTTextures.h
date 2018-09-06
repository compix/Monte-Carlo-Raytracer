#pragma once

struct RTTextureDesc2D
{
	uint16_t width;
	uint16_t height;
	uint16_t numMipLevels;
	uint16_t format;
	uint16_t wrap;
	uint16_t pad;
	uint32_t memOffset; // Byte offset
};

enum RTTextureWrapping
{
	RT_TEX_WRAP_REPEAT,
	RT_TEX_WRAP_MIRRORED_REPEAT,
	RT_TEX_WRAP_CLAMP_TO_EDGE,
	RT_TEX_WRAP_CLAMP_TO_BORDER
};

enum RTTextureFormat
{
	RT_TEX_FORMAT_R8,
	RT_TEX_FORMAT_RG8,
	RT_TEX_FORMAT_RGB8,
	RT_TEX_FORMAT_RGBA8
};