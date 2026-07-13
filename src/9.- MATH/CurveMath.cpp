// Implementación de cálculos de curvas.
// Proporciona posiciones interpoladas para movimientos complejos.

#include "9.- MATH/CurveMath.h"

#include <algorithm>

namespace Math
{
	RE::NiPoint3 EvaluateQuadraticBezier(const RE::NiPoint3& a_start, const RE::NiPoint3& a_control, const RE::NiPoint3& a_end, float a_t)
	{
		const float t = std::clamp(a_t, 0.0f, 1.0f);
		const float u = 1.0f - t;

		return a_start * (u * u) + a_control * (2.0f * u * t) + a_end * (t * t);
	}

	RE::NiPoint3 EvaluateQuadraticBezierTangent(const RE::NiPoint3& a_start, const RE::NiPoint3& a_control, const RE::NiPoint3& a_end, float a_t)
	{
		const float t = std::clamp(a_t, 0.0f, 1.0f);

		return (a_control - a_start) * (2.0f * (1.0f - t)) + (a_end - a_control) * (2.0f * t);
	}
}
