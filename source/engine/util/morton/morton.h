#pragma once
#include <cstdint>
#include <cassert>

namespace morton
{
    /**
     * @fn  inline uint64_t separateBy3(uint32_t n)
     *
     * @brief   Separate the bits of n by 3 by adding 00 between bits. This is the inverse operation of {@link #compactBy3(uint64_t) compactBy3}.
     *          Example with 4 bits: 1101 -> 1|001|000|001
     */

    inline uint64_t separateBy3(uint32_t n)
    {
        uint64_t x = n & 0x1fffff;
        x = (x | x << 32) & 0x1f00000000ffff;
        x = (x | x << 16) & 0x1f0000ff0000ff;
        x = (x | x << 8) & 0x100f00f00f00f00f;
        x = (x | x << 4) & 0x10c30c30c30c30c3;
        x = (x | x << 2) & 0x1249249249249249;
        return x;
    }

    /**
     * @fn  inline uint64_t compactBy3(uint64_t n)
     *
     * @brief   Compact the bits of n by 3 by removing 2 consecutive bits. This is the inverse operation of {@link #separateBy3(uint64_t) separateBy3}.
     *          Example: 1|001|000|001 -> 1101
     */

    inline uint64_t compactBy3(uint64_t n)
    {
        n &= 0x1249249249249249;
        n = (n ^ n >> 2) & 0x30c30c30c30c30c3;
        n = (n ^ n >> 4) & 0xf00f00f00f00f00f;
        n = (n ^ n >> 8) & 0x00ff0000ff0000ff;
        n = (n ^ n >> 16) & 0x00ff00000000ffff;
        n = (n ^ n >> 32) & 0x1fffff;
        return n;
    }

	inline uint32_t compactBy3(uint32_t x)
	{
		x &= 0x09249249;             
		x = (x ^ (x >> 2)) & 0x030c30c3;
		x = (x ^ (x >> 4)) & 0x0300f00f;
		x = (x ^ (x >> 8)) & 0xff0000ff;
		x = (x ^ (x >> 16)) & 0x000003ff;
		return x;
	}

    /**
     * @fn  inline uint64_t encode(uint32_t x, uint32_t y, uint32_t z)
     *
     * @brief   Encodes the given grid coordinates (x, y, z) in morton order.
     *          Note: The coordinates must not exceed 21 bits.
     *
     * @return  The morton encoded coordinates.
     */

    inline uint64_t encode63(uint32_t x, uint32_t y, uint32_t z)
    {
        assert(x <= 0x1fffff && y <= 0x1fffff && z <= 0x1fffff);
        return separateBy3(x) | separateBy3(y) << 1 | separateBy3(z) << 2;
    }

    /**
     * @fn  inline void decode(uint64_t m, uint32_t& x, uint32_t& y, uint32_t& z)
     *
     * @brief   Decodes the given morton encoded coordinates m and outputs grid coordinates (x, y, z).
     *
     * @param   m           The morton encoded coordinates.
     */

    inline void decode63(uint64_t m, uint32_t& x, uint32_t& y, uint32_t& z)
    {
        x = static_cast<uint32_t>(compactBy3(m));
        y = static_cast<uint32_t>(compactBy3(m >> 1));
        z = static_cast<uint32_t>(compactBy3(m >> 2));
    }

	inline uint32_t encode30(uint32_t x, uint32_t y, uint32_t z)
	{
		x = (x | (x << 16)) & 0x030000FF;
		x = (x | (x << 8)) & 0x0300F00F;
		x = (x | (x << 4)) & 0x030C30C3;
		x = (x | (x << 2)) & 0x09249249;

		y = (y | (y << 16)) & 0x030000FF;
		y = (y | (y << 8)) & 0x0300F00F;
		y = (y | (y << 4)) & 0x030C30C3;
		y = (y | (y << 2)) & 0x09249249;

		z = (z | (z << 16)) & 0x030000FF;
		z = (z | (z << 8)) & 0x0300F00F;
		z = (z | (z << 4)) & 0x030C30C3;
		z = (z | (z << 2)) & 0x09249249;

		return x | (y << 1) | (z << 2);
	}

	inline void decode30(uint32_t code, uint32_t& x, uint32_t& y, uint32_t& z)
    {
		x = compactBy3(code);
		y = compactBy3(code >> 1);
		z = compactBy3(code >> 2);
    }

	inline uint64_t encode60(uint32_t x, uint32_t y,uint32_t z)
	{
		uint32_t lowX = x & 1023u;
		uint32_t lowY = y & 1023u;
		uint32_t lowZ = z & 1023u;
		uint32_t highX = x >> 10u;
		uint32_t highY = y >> 10u;
		uint32_t highZ = z >> 10u;

		return (static_cast<uint64_t>(encode30(highX, highY, highZ)) << 30) | 
			    static_cast<uint64_t>(encode30(lowX, lowY, lowZ));
	}
}

template<typename IntType>
struct MortonCode3D {};

template<>
struct MortonCode3D<uint32_t>
{
	MortonCode3D() {}
	MortonCode3D(uint32_t x, uint32_t y, uint32_t z) : value(morton::encode30(x, y, z)) {}
	~MortonCode3D() {}

	operator uint32_t() const
	{
		return value;
	}

	void decode(uint32_t x, uint32_t y, uint32_t z) const
	{
		morton::decode30(value, x, y, z);
	}

	uint32_t value{0};
	static const uint32_t order = 10;
	static const uint32_t usedBitCount = 30;
	static const uint32_t totalBitCount = 32;
};

template<>
struct MortonCode3D<uint64_t>
{
	MortonCode3D() {}
	MortonCode3D(uint32_t x, uint32_t y, uint32_t z) : value(morton::encode63(x, y, z)) {}
	~MortonCode3D() {}

	operator uint64_t() const
	{
		return value;
	}

	void decode(uint32_t x, uint32_t y, uint32_t z) const
	{
		morton::decode63(value, x, y, z);
	}

	uint64_t value{0};
	static const uint32_t order = 21;
	static const uint32_t usedBitCount = 63;
	static const uint32_t totalBitCount = 64;
};
