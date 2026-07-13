// Implementación del cálculo de trayectoria de retorno (fase 1: línea recta
// con aceleración híbrida).

#include "5.- RETURN/ReturnTrajectory.h"

#include <cmath>

namespace Return
{
	RE::NiPoint3 ComputeNextPosition(const RE::NiPoint3& a_current, const RE::NiPoint3& a_target, float a_speed, float a_deltaTime)
	{
		const RE::NiPoint3 toTarget = a_target - a_current;
		const float        distance = toTarget.Length();
		const float        step = a_speed * a_deltaTime;

		if (distance <= 0.0f || step >= distance) {
			return a_target;
		}

		return a_current + toTarget * (step / distance);
	}

	float ComputeReturnAcceleration(float a_distance, float a_defaultAcceleration, float a_maxDuration)
	{
		if (a_defaultAcceleration <= 0.0f || a_maxDuration <= 0.0f || a_distance <= 0.0f) {
			return a_defaultAcceleration;
		}

		// distancia = ½·a·t² (velocidad inicial cero) => t = √(2·d/a).
		const float durationAtDefault = std::sqrt(2.0f * a_distance / a_defaultAcceleration);
		if (durationAtDefault <= a_maxDuration) {
			return a_defaultAcceleration;
		}

		// Despejando a de la misma fórmula para t = a_maxDuration.
		return 2.0f * a_distance / (a_maxDuration * a_maxDuration);
	}
}
