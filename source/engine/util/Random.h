#pragma once

#include <random>
#include <glm/detail/type_vec2.hpp>
#include <glm/detail/type_vec3.hpp>
#include <glm/detail/type_vec4.hpp>

/**
Uses the std::mt19937 random number generator and uniform distributions.
*/
class Random
{
public:
    static void randomize();

    static void seed(unsigned int seed) { m_rng.seed(seed); }

    /**
    Returns a random int between start (included) and end (included).
    */
    static int getInt(int start, int end);

    /**
     * Returns a random uint32_t between start (included) and end (included).
     */
    static uint32_t getUInt(uint32_t start, uint32_t end);


    /**
    Returns a random float in [start, end).
    */
    static float getFloat(float start, float end);

	/**
	 * Returns a random 2D vector with values in [start, end).
	 */
	static glm::vec2 getVec2(float start, float end);

	/**
	* Returns a random 3D vector with values in [start, end).
	*/
	static glm::vec3 getVec3(float start, float end);

	/**
	* Returns a random 4D vector with values in [start, end).
	*/
	static glm::vec4 getVec4(float start, float end);
private:
    static std::mt19937 m_rng;
};
