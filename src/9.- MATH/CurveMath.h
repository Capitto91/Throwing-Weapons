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

	// Interpolación lineal entre a_p0 (a_t=0) y a_p1 (a_t=1). Usada por
	// 8.- ANIMATION/WeaponTrail para reposicionar los segmentos de la
	// estela entre las 2 últimas muestras del historial de posiciones de
	// la réplica -- 2026-08-27, sustituye a un Catmull-Rom anterior
	// (retirado, sin más uso): para el efecto rayo, la curva C1-continua de
	// Catmull-Rom redondeaba los quiebros del historial en vez de
	// marcarlos, justo lo contrario de lo que pide un rayo (ver
	// WeaponTrail.cpp para el detalle completo).
	RE::NiPoint3 Lerp(const RE::NiPoint3& a_p0, const RE::NiPoint3& a_p1, float a_t);
}
