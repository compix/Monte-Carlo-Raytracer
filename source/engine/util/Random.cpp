#include "Random.h"
#include <float.h>

std::mt19937 Random::m_rng;

void Random::randomize()
{
    std::random_device rd;
    m_rng.seed(rd());
}

int Random::getInt(int start, int end)
{
    std::uniform_int_distribution<int> distribution(start, end);
    return distribution(m_rng);
}

uint32_t Random::getUInt(uint32_t start, uint32_t end)
{
    std::uniform_int_distribution<uint32_t> distribution(start, end);
    return distribution(m_rng);
}

float Random::getFloat(float start, float end)
{
    std::uniform_real_distribution<float> distribution(start, end);
    return distribution(m_rng);
}

glm::vec2 Random::getVec2(float start, float end)
{
	std::uniform_real_distribution<float> distribution(start, end);
	return glm::vec2(distribution(m_rng), distribution(m_rng));
}

glm::vec3 Random::getVec3(float start, float end)
{
	std::uniform_real_distribution<float> distribution(start, end);
	return glm::vec3(distribution(m_rng), distribution(m_rng), distribution(m_rng));
}

glm::vec4 Random::getVec4(float start, float end)
{
	std::uniform_real_distribution<float> distribution(start, end);
	return glm::vec4(distribution(m_rng), distribution(m_rng), distribution(m_rng), distribution(m_rng));
}
