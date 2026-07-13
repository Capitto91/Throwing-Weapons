// Cálculo matemático de la trayectoria de retorno.
// Fase 1: línea recta hacia la mano del jugador. Pendiente: curvatura
// obligatoria (punto 7), velocidad híbrida a 2s (punto 8), homing con
// ángulo máximo (punto 10) y enderezado antes de llegar (punto 11) de
// Mecanica del arma.txt.

#pragma once

namespace Return
{
	// Devuelve la siguiente posición al avanzar desde a_current hacia
	// a_target, como mucho a_speed unidades por segundo durante
	// a_deltaTime. Nunca sobrepasa a_target, aunque el paso calculado sea
	// mayor que la distancia restante.
	[[nodiscard]] RE::NiPoint3 ComputeNextPosition(const RE::NiPoint3& a_current, const RE::NiPoint3& a_target, float a_speed, float a_deltaTime);
}
