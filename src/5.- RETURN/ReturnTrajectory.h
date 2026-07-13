// Cálculo matemático de la trayectoria de retorno.
// Fase 1: línea recta hacia la mano del jugador, con aceleración híbrida
// (punto 8 de Mecanica del arma.txt). Pendiente: curvatura obligatoria
// (punto 7), homing con ángulo máximo (punto 10) y enderezado antes de
// llegar (punto 11).

#pragma once

namespace Return
{
	// Devuelve la siguiente posición al avanzar desde a_current hacia
	// a_target, como mucho a_speed unidades por segundo durante
	// a_deltaTime. Nunca sobrepasa a_target, aunque el paso calculado sea
	// mayor que la distancia restante.
	[[nodiscard]] RE::NiPoint3 ComputeNextPosition(const RE::NiPoint3& a_current, const RE::NiPoint3& a_target, float a_speed, float a_deltaTime);

	// Aceleración a aplicar durante todo el regreso (punto 8 de Mecanica
	// del arma.txt, partiendo siempre de velocidad cero): a_distance es la
	// distancia total a recorrer, capturada al empezar el regreso. Se usa
	// a_defaultAcceleration salvo que, partiendo del reposo a esa
	// aceleración, recorrer a_distance tardara más de a_maxDuration
	// segundos — en ese caso devuelve la aceleración mínima necesaria para
	// completarlo en exactamente a_maxDuration (distancia = ½·a·t²).
	[[nodiscard]] float ComputeReturnAcceleration(float a_distance, float a_defaultAcceleration, float a_maxDuration);
}
