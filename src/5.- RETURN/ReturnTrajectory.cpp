// Implementación del cálculo de trayectoria de retorno (fase 1: línea recta).

#include "5.- RETURN/ReturnTrajectory.h"

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
}
