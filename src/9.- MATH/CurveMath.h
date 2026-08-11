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

	// Interpola entre a_p1 y a_p2 (a_p0/a_p3 son los puntos anterior y
	// siguiente de la secuencia, que dan forma a la tangente) en a_t (0 =
	// a_p1, 1 = a_p2). Usada por 8.- ANIMATION/WeaponTrail para suavizar el
	// historial de posiciones de la réplica al reposicionar los segmentos
	// de la estela -- portado tal cual de Precision (Ershin, MIT License,
	// github.com/ersh1/Precision, src/Utils.cpp, Utils::CatmullRom).
	RE::NiPoint3 CatmullRom(const RE::NiPoint3& a_p0, const RE::NiPoint3& a_p1, const RE::NiPoint3& a_p2, const RE::NiPoint3& a_p3, float a_t);
}
