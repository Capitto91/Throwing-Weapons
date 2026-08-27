// Implementación de WeaponTrailGroup -- ver WeaponTrailGroup.h para el diseño.

#include "8.- ANIMATION/WeaponTrailGroup.h"

#include "1.- CORE/Constants.h"

#include <algorithm>
#include <numbers>

namespace Animation
{
	namespace
	{
		// Roll (radianes) de la copia a_index: a_baseRoll más
		// a_index·Constants::kTrailCopyRollStepDegrees, convertido a
		// radianes. Misma fórmula usada por Start y SetRoll, para que el
		// desfase entre copias no cambie de significado entre una llamada
		// y otra.
		float ComputeCopyRoll(float a_baseRoll, std::size_t a_index)
		{
			return a_baseRoll + static_cast<float>(a_index) * Constants::kTrailCopyRollStepDegrees * std::numbers::pi_v<float> / 180.0f;
		}
	}

	WeaponTrailGroup::WeaponTrailGroup()
	{
		trails.resize(Constants::kTrailCopyCount);
		heldDeviationRight.resize(Constants::kTrailCopyCount, 0.0f);
		heldDeviationUp.resize(Constants::kTrailCopyCount, 0.0f);
		holdTimers.resize(Constants::kTrailCopyCount, 0.0f);
	}

	void WeaponTrailGroup::Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset)
	{
		for (std::size_t i = 0; i < trails.size(); ++i) {
			trails[i].Start(a_cell, a_initialPosition, a_upReference, ComputeCopyRoll(a_roll, i), a_anchorWorldOffset);
		}

		previousRawPosition.reset();

		// Fuerza un resorteo inmediato en el primer Update() -- un tramo
		// nuevo no debe heredar el desvío mantenido del tramo anterior.
		std::ranges::fill(heldDeviationRight, 0.0f);
		std::ranges::fill(heldDeviationUp, 0.0f);
		std::ranges::fill(holdTimers, Constants::kTrailLightningHoldSeconds);
	}

	void WeaponTrailGroup::SetRoll(float a_roll)
	{
		for (std::size_t i = 0; i < trails.size(); ++i) {
			trails[i].SetRoll(ComputeCopyRoll(a_roll, i));
		}
	}

	void WeaponTrailGroup::Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds)
	{
		// Efecto rayo (ver cabecera de WeaponTrailGroup.h) -- base
		// perpendicular a la dirección de avance REAL (sin desviar),
		// calculada una única vez por tick; el sorteo del desvío en sí
		// pasa a hacerse POR COPIA dentro del bucle de más abajo (cambio
		// 2026-08-26, a petición del usuario: compartir un único desvío
		// entre las 8 copias se notaba como que "todas se desvían de la
		// misma manera" -- con sorteo independiente por copia, cada una
		// traza su propio zigzag, aunque las 8 sigan centradas en la
		// misma trayectoria real de fondo). Dirección estimada por
		// diferencia con la posición del tick anterior (sin ella todavía,
		// primer tick tras Start(), no hay desviación posible).
		RE::NiPoint3 right{ 1.0f, 0.0f, 0.0f };
		RE::NiPoint3 up{ 0.0f, 0.0f, 1.0f };
		bool         hasDeviationBasis = false;

		if (previousRawPosition.has_value()) {
			RE::NiPoint3 travelDir = a_currentPosition - *previousRawPosition;
			const float  travelLength = travelDir.Length();
			if (travelLength > 0.0f) {
				travelDir = travelDir / travelLength;

				// Gram-Schmidt contra el eje Z del mundo -- mismo
				// convenio de respaldo que Math::SetRotationFromForwardUp
				// para direcciones (casi) verticales.
				right = travelDir.Cross(RE::NiPoint3{ 0.0f, 0.0f, 1.0f });
				float rightLength = right.Length();
				if (rightLength < 1.0e-4f) {
					right = travelDir.Cross(RE::NiPoint3{ 1.0f, 0.0f, 0.0f });
					rightLength = right.Length();
				}
				right = rightLength > 0.0f ? right / rightLength : RE::NiPoint3{ 1.0f, 0.0f, 0.0f };
				up = travelDir.Cross(right);
				hasDeviationBasis = true;
			}
		}

		previousRawPosition = a_currentPosition;

		std::uniform_real_distribution<float> jitterDist(-Constants::kTrailLightningMaxDeviation, Constants::kTrailLightningMaxDeviation);

		for (std::size_t i = 0; i < trails.size(); ++i) {
			// Resorteo por copia solo al agotarse su propio holdTimer (ver
			// cabecera del archivo) -- el resto de ticks reutiliza la misma
			// magnitud por eje, así que el punto de quiebro en el
			// historial de WeaponTrail no aparece cada 16ms.
			holdTimers[i] += a_deltaSeconds;
			if (holdTimers[i] >= Constants::kTrailLightningHoldSeconds) {
				holdTimers[i] -= Constants::kTrailLightningHoldSeconds;
				heldDeviationRight[i] = jitterDist(randomEngine);
				heldDeviationUp[i] = jitterDist(randomEngine);
			}

			RE::NiPoint3 jitteredPosition = a_currentPosition;
			if (hasDeviationBasis) {
				jitteredPosition = jitteredPosition + right * heldDeviationRight[i] + up * heldDeviationUp[i];
			}
			trails[i].Update(jitteredPosition, a_deltaSeconds);
		}
	}
}
