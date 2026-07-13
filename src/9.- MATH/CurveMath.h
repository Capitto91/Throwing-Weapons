// Biblioteca matemática para generación de trayectorias.
// Contiene funciones para curvas Bezier, interpolaciones y cálculos espaciales.

#pragma once

namespace Math
{
	// Evalúa una curva de Bezier cuadrática en a_t (0 = a_start, 1 = a_end),
	// con a_control como punto de control intermedio. Usada por
	// Return::BeginReturn para la curvatura obligatoria del regreso (punto 7
	// de Mecanica del arma.txt: nunca en línea recta).
	[[nodiscard]] RE::NiPoint3 EvaluateQuadraticBezier(const RE::NiPoint3& a_start, const RE::NiPoint3& a_control, const RE::NiPoint3& a_end, float a_t);

	// Derivada (tangente, sin normalizar) de la curva de arriba en a_t:
	// dirección de avance real sobre la curva, no la línea recta hacia
	// a_end. Usada para orientar la réplica durante el vuelo (punto 11 de
	// Mecanica del arma.txt).
	[[nodiscard]] RE::NiPoint3 EvaluateQuadraticBezierTangent(const RE::NiPoint3& a_start, const RE::NiPoint3& a_control, const RE::NiPoint3& a_end, float a_t);
}
