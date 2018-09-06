/**
* It is important to understand how PRNGs 
* (pseudo random number generators - shortly referred to by RNG) work
* to use them correctly. A PRNG expects a seed value and outputs a random number.
* The seed is used as the state of the RNG and is changed in the RNG-function.
* Example usage:
* uint threadId = get_global_id(1) * screenWidth + get_global_id(0);
* uint seed = threadId;
* uint randomNumber0 = randUInt(seed);
* uint randomNumber1 = randUInt(seed);
* // ...
*
* Many PRNGs perform well in the above example generating sequences but have correlations between different seed values
* resulting in visible patterns if used for Monte Carlo integration. To mitigate this issue it is necessary
* to generate random seed values. A fast hash function with random properties can be used (e.g. Wang Hash).
* The proper implementation of the above example is thus:
* uint threadId = get_global_id(1) * screenWidth + get_global_id(0);
* uint seed = randSequenceSeed(threadId);
* uint randomNumber0 = randUInt(seed);
* uint randomNumber1 = randUInt(seed);
* // ...
*/

#ifndef RNG_CLH
#define RNG_CLH

#define LCG_RNG 0
#define XORSHIFT_RNG 1
#define WANG_HASH_RNG 2
#define WANG_HASH_AND_LCG 3 // wang_hash(lcg(seed))
#define IDENTITY_RNG 4 // Just returns the seed; not random

#define SEQUENCE_RNG XORSHIFT_RNG
#define SEED_RNG WANG_HASH_RNG

/**
* Linear congruential generator.
*/
uint randUInt_lcg(uint* seed)
{
    *seed = 1664525 * (*seed) + 1013904223;
    return *seed;
}

/**
* RNG from George Marsaglia "Xorshift RNGs"
*/
uint randUInt_xorshift(uint* seed)
{
	// Adding 2463534242 because an initial seed of 0 creates a thin black line
	*seed += 2463534242;
    *seed ^= ((*seed) << 13);
    *seed ^= ((*seed) >> 17);
    *seed ^= ((*seed) << 5);
    return *seed;
}

uint wang_hash(uint seed)
{
    seed = (seed ^ 61) ^ (seed >> 16);
    seed *= 9;
    seed = seed ^ (seed >> 4);
    seed *= 0x27d4eb2d;
    seed = seed ^ (seed >> 15);
    return seed;
}

#if SEQUENCE_RNG == XORSHIFT_RNG
uint randUInt(uint* seed)
{
	return randUInt_xorshift(seed);
}
#elif SEQUENCE_RNG == LCG_RNG
uint randUInt(uint* seed)
{
	return randUInt_lcg(seed);
}
#elif SEQUENCE_RNG == WANG_HASH_AND_LCG
uint randUInt(uint* seed)
{
	*seed = wang_hash(randUInt_lcg(seed));
	return *seed;
}
#elif SEQUENCE_RNG == WANG_HASH_RNG
uint randUInt(uint* seed)
{
	*seed = wang_hash(*seed);
	return *seed;
}
#endif

#if SEED_RNG == WANG_HASH_RNG
uint randSequenceSeed(uint seed)
{
	return wang_hash(seed);
}
#elif SEED_RNG == IDENTITY_RNG
uint randSequenceSeed(uint seed)
{
	return seed;
}
#endif

float randFloat(uint* seed)
{
	return ((float)randUInt(seed)) / 0xffffffff; 
}

/**
* Launch2D with global size of at least (width, height)
*/
__kernel void generateNoiseTexture(
                uint width,
                uint height,
				uint numChannels,
                __global unsigned char* outTexture)
{
    uint2 globalid;
    globalid.x  = get_global_id(0);
    globalid.y  = get_global_id(1);
	
	if (globalid.x < width && globalid.y < height)
	{
		uint threadId = globalid.y * width + globalid.x;
		uint seed = randSequenceSeed(threadId);
		unsigned char noise = randUInt(&seed) % 256;
		for (uint j = 0; j < numChannels; ++j)
			outTexture[threadId * numChannels + j] = noise;
	}
}

__kernel void generateNoiseTextureTestSequence(
                uint width,
                uint height,
				uint numChannels, // Number of color channels (RGB = 3 color channels)
                __global unsigned char* outTexture)
{
    uint threadId = get_global_id(0);
	uint seed = randSequenceSeed(threadId);

	// Iterates over width. Launch1D with height as global size.
	for (uint i = 0; i < width; ++i)
	{
		unsigned char noise = randUInt(&seed) % 256;
		for (uint j = 0; j < numChannels; ++j)
			outTexture[(threadId * width + i) * numChannels + j] = noise;
	}

	// Iterates over width and height. Launch1D with 1 as global size.
	//for (uint x = 0; x < width; ++x)
	//{
	//	for (uint y = 0; y < height; ++y)
	//	{
	//		unsigned char noise = randUInt(&seed) % 256;
	//		for (uint j = 0; j < numChannels; ++j)
	//			outTexture[(y * width + x) * numChannels + j] = noise;
	//	}
	//}
}

#endif