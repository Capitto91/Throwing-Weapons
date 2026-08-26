// Implementación de WeaponTrailGroup -- ver WeaponTrailGroup.h para el diseño.

#include "8.- ANIMATION/WeaponTrailGroup.h"

#include "1.- CORE/Constants.h"

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
	}

	void WeaponTrailGroup::Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset)
	{
		for (std::size_t i = 0; i < trails.size(); ++i) {
			trails[i].Start(a_cell, a_initialPosition, a_upReference, ComputeCopyRoll(a_roll, i), a_anchorWorldOffset);
		}
	}

	void WeaponTrailGroup::SetRoll(float a_roll)
	{
		for (std::size_t i = 0; i < trails.size(); ++i) {
			trails[i].SetRoll(ComputeCopyRoll(a_roll, i));
		}
	}

	void WeaponTrailGroup::Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds)
	{
		for (auto& trail : trails) {
			trail.Update(a_currentPosition, a_deltaSeconds);
		}
	}
}
