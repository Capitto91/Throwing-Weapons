// Copias de la estela -- ver WeaponTrailGroup.h.

#include "8.- ANIMATION/WeaponTrailGroup.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Settings.h"
#include "9.- MATH/RotationMath.h"

#include <algorithm>

namespace Animation
{
	namespace
	{
		// Roll de la copia a_index en radianes.
		float ComputeCopyRoll(float a_baseRoll, std::size_t a_index)
		{
			return a_baseRoll + static_cast<float>(a_index) * Math::DegreesToRadians(Constants::kTrailCopyRollStepDegrees);
		}
	}

	WeaponTrailGroup::WeaponTrailGroup()
	{
		trails.resize(Constants::kTrailCopyCount);
		heldDeviationRight.resize(Constants::kTrailCopyCount, 0.0f);
		heldDeviationUp.resize(Constants::kTrailCopyCount, 0.0f);
		holdTimers.resize(Constants::kTrailCopyCount, 0.0f);
	}

	void WeaponTrailGroup::Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset, bool a_checkSetting)
	{
		enabled = !a_checkSetting || Settings::GetTrail();
		if (!enabled) {
			return;
		}

		for (std::size_t i = 0; i < trails.size(); ++i) {
			trails[i].Start(a_cell, a_initialPosition, a_upReference, ComputeCopyRoll(a_roll, i), a_anchorWorldOffset);
		}

		previousRawPosition.reset();

		// Fuerza un sorteo nuevo en el primer Update.
		std::ranges::fill(heldDeviationRight, 0.0f);
		std::ranges::fill(heldDeviationUp, 0.0f);
		std::ranges::fill(holdTimers, Constants::kTrailLightningHoldSeconds);
	}

	void WeaponTrailGroup::SetRoll(float a_roll)
	{
		if (!enabled) {
			return;
		}

		for (std::size_t i = 0; i < trails.size(); ++i) {
			trails[i].SetRoll(ComputeCopyRoll(a_roll, i));
		}
	}

	void WeaponTrailGroup::Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds)
	{
		if (!enabled) {
			return;
		}

		// Base perpendicular a la dirección real de avance (por diferencia con el tick anterior).
		RE::NiPoint3 right{ 1.0f, 0.0f, 0.0f };
		RE::NiPoint3 up{ 0.0f, 0.0f, 1.0f };
		bool         hasDeviationBasis = false;

		if (previousRawPosition.has_value()) {
			RE::NiPoint3 travelDir = a_currentPosition - *previousRawPosition;
			const float  travelLength = travelDir.Length();
			if (travelLength > 0.0f) {
				travelDir = travelDir / travelLength;

				// Respaldo con el eje Z del mundo para direcciones casi verticales.
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
			// Nuevo sorteo de cada copia solo al agotarse su holdTimer.
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
