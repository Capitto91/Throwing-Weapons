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
}
