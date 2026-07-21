// Biblioteca matemática para generación de trayectorias.
// Contiene funciones para curvas Bezier, interpolaciones y cálculos espaciales.

#pragma once

// Punto 7 de Mecanica del arma.txt: el regreso nunca sigue una línea
// recta. Curva de Bezier cuadrática genérica (no depende de nada
// específico del arma ni del jugador, ver 5.- RETURN/ReturnTrajectory
// para el cálculo del punto de control concreto del regreso).

namespace Math
{
	// Evalúa la curva de Bezier cuadrática definida por a_p0 (inicio),
	// a_control (punto de control) y a_p2 (fin) en a_t (0 = inicio, 1 =
	// fin). Fórmula estándar: (1-t)²·p0 + 2·(1-t)·t·control + t²·p2.
	RE::NiPoint3 EvaluateQuadraticBezier(const RE::NiPoint3& a_p0, const RE::NiPoint3& a_control, const RE::NiPoint3& a_p2, float a_t);
}
