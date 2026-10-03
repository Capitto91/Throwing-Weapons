// Varias copias de WeaponTrail giradas entre sí (Constants::kTrailCopyRollStepDegrees)
// con desvío aleatorio propio cada una para el efecto rayo. La usan Throw y Return.

#pragma once

#include "8.- ANIMATION/WeaponTrail.h"

#include <optional>
#include <random>
#include <vector>

namespace Animation
{
	class WeaponTrailGroup
	{
	public:
		// Reserva Constants::kTrailCopyCount estelas sin arrancar.
		WeaponTrailGroup();

		// Arranca todas las copias; a_roll es el de la copia 0, las demás suman su desfase.
		// Con a_checkSetting=false arranca aunque [VFX] Trail esté desactivado (LightningDash).
		void Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset, bool a_checkSetting = true);

		// Cambia el roll base de todas las copias.
		void SetRoll(float a_roll);

		// Mueve cada copia a a_currentPosition con su propio desvío aleatorio.
		void Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds);

	private:
		// [VFX] Trail leído en Start (o true si no se consulta): con false no hace nada en ese tramo.
		bool                        enabled{ false };
		std::vector<WeaponTrail>    trails;
		std::mt19937                randomEngine{ std::random_device{}() };
		std::optional<RE::NiPoint3> previousRawPosition;

		// Desvío mantenido por copia y tiempo desde el último sorteo.
		std::vector<float> heldDeviationRight;
		std::vector<float> heldDeviationUp;
		std::vector<float> holdTimers;
	};
}
