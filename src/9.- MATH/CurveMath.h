// Curva de Bezier cuadrática e interpolación lineal entre puntos.

#pragma once

namespace Math
{
	// Punto de la Bezier a_p0 -> a_p2 con control a_control en a_t (0..1).
	// La usa ReturnTrajectory para la curva del regreso.
	RE::NiPoint3 EvaluateQuadraticBezier(const RE::NiPoint3& a_p0, const RE::NiPoint3& a_control, const RE::NiPoint3& a_p2, float a_t);

	// Interpolación lineal entre a_p0 (a_t=0) y a_p1 (a_t=1).
	// La usa WeaponTrail para colocar los segmentos de la estela.
	RE::NiPoint3 Lerp(const RE::NiPoint3& a_p0, const RE::NiPoint3& a_p1, float a_t);
}
