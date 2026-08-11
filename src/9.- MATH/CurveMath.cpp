// Implementación de cálculos de curvas.
// Proporciona posiciones interpoladas para movimientos complejos.

#include "9.- MATH/CurveMath.h"

namespace Math
{
	RE::NiPoint3 EvaluateQuadraticBezier(const RE::NiPoint3& a_p0, const RE::NiPoint3& a_control, const RE::NiPoint3& a_p2, float a_t)
	{
		const float u = 1.0f - a_t;
		return a_p0 * (u * u) + a_control * (2.0f * u * a_t) + a_p2 * (a_t * a_t);
	}

	RE::NiPoint3 CatmullRom(const RE::NiPoint3& a_p0, const RE::NiPoint3& a_p1, const RE::NiPoint3& a_p2, const RE::NiPoint3& a_p3, float a_t)
	{
		const RE::NiPoint3 a = a_p1 * 2.0f;
		const RE::NiPoint3 b = a_p2 - a_p0;
		const RE::NiPoint3 c = a_p0 * 2.0f - a_p1 * 5.0f + a_p2 * 4.0f - a_p3;
		const RE::NiPoint3 d = -a_p0 + a_p1 * 3.0f - a_p2 * 3.0f + a_p3;

		return (a + (b * a_t) + (c * a_t * a_t) + (d * a_t * a_t * a_t)) * 0.5f;
	}
}
