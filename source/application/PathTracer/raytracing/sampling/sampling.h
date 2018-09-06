#pragma once
#include "sobol.h"

class Sampler
{
public:
	static inline float sobolSample(uint32_t idx, uint32_t dimension, uint32_t scramble)
	{
		uint32_t v = scramble;
		for (uint32_t i = dimension * SOBOL_MATRIX_SIZE; idx != 0; idx >>= 1, ++i)
			if (idx & 1)
				v ^= g_SobolMatrices32[i];

		return v * 0x1p-32f;
	}

	static glm::vec2 concentricSampleDisc(const glm::vec2& u)
	{
		// Map to [-1,1]^2
		glm::vec2 uOffset = 2.0f * u - glm::vec2(1.0f);

		if (u.x < 1e-8f && u.y < 1e-8f)
			return glm::vec2(0.0f);

		float theta, r;

		if (std::fabs(uOffset.x) > std::fabs(uOffset.y))
		{
			r = uOffset.x;
			theta = math::PI_DIV_4 * (uOffset.y / uOffset.x);
		}
		else
		{
			r = uOffset.y;
			theta = math::PI_DIV_2 - math::PI_DIV_4 * (uOffset.x / uOffset.y);
		}

		return r * glm::vec2(std::cos(theta), std::sin(theta));
	}

	static glm::vec2 uniformSampleDisc(const glm::vec2& u)
	{
		float r = std::sqrt(u.x);
		float theta = 2.0f * PI * u.y;

		return glm::vec2(r * std::cos(theta), r * std::sin(theta));
	}
};