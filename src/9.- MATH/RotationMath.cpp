// Implementación de cálculos de rotación.
// Gestiona quaterniones, orientación hacia objetivos y ajuste del mango.

#include "9.- MATH/RotationMath.h"

#include <algorithm>
#include <cmath>

namespace Math
{
	RE::NiPoint3 ComputeLookAtAngles(const RE::NiPoint3& a_direction)
	{
		RE::NiPoint3 direction = a_direction;
		if (direction.Unitize() <= 0.0f) {
			return { 0.0f, 0.0f, 0.0f };
		}

		const float pitch = -std::asin(std::clamp(direction.z, -1.0f, 1.0f));
		const float yaw = std::atan2(direction.x, direction.y);

		return { pitch, 0.0f, yaw };
	}

	float LerpAngle(float a_from, float a_to, float a_t)
	{
		constexpr float kPi = 3.14159265358979323846f;
		constexpr float kTwoPi = 2.0f * kPi;

		float delta = std::fmod(a_to - a_from + kPi, kTwoPi);
		if (delta < 0.0f) {
			delta += kTwoPi;
		}
		delta -= kPi;

		return a_from + delta * a_t;
	}
}
