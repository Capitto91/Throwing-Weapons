// Implementación del cálculo de trayectoria de retorno (curva de Bezier
// cuadrática con aceleración híbrida).

#include "5.- RETURN/ReturnTrajectory.h"

#include "1.- CORE/Constants.h"

#include <algorithm>
#include <cmath>

namespace Return
{
	RE::NiPoint3 ComputeReturnControlPoint(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, const RE::NiPoint3& a_preferredSide)
	{
		const RE::NiPoint3 toEnd = a_end - a_start;
		const float        distance = toEnd.Length();
		if (distance <= 0.0f) {
			return a_start;
		}

		const RE::NiPoint3 forward = toEnd / distance;

		// Componente de a_preferredSide perpendicular a la línea recta
		// (proyección rechazada, Gram-Schmidt): el arma se recoge con la
		// mano derecha, así que se pasa el vector "derecha" del jugador
		// para que la curva entre siempre por ese lado en vez de uno
		// arbitrario.
		RE::NiPoint3 side = a_preferredSide - forward * forward.Dot(a_preferredSide);
		if (side.SqrLength() <= 0.0001f) {
			// a_preferredSide casi paralelo a la línea recta (caso
			// degenerado, p. ej. clavado justo detrás/delante del
			// jugador): cualquier perpendicular horizontal sirve para
			// seguir cumpliendo la curvatura obligatoria del punto 7.
			static const RE::NiPoint3 kWorldUp{ 0.0f, 0.0f, 1.0f };
			side = forward.UnitCross(kWorldUp);
			if (side.SqrLength() <= 0.0001f) {
				side = { 1.0f, 0.0f, 0.0f };
			}
		} else {
			side.Unitize();
		}

		const float        offset = std::clamp(distance * Constants::kReturnCurveLateralFraction, Constants::kReturnCurveMinOffset, Constants::kReturnCurveMaxOffset);
		const RE::NiPoint3 midpoint = a_start + forward * (distance * 0.5f);

		return midpoint + side * offset;
	}

	float ComputeTraveledDistance(float a_acceleration, float a_elapsedTime)
	{
		return 0.5f * a_acceleration * a_elapsedTime * a_elapsedTime;
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
