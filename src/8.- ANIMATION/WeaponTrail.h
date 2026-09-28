// Estela de rayo (Constants::kTrailEffectPath, BSTempEffectParticle) cuyos huesos se recolocan
// cada tick sobre el historial de posiciones de la réplica. Basado en la estela de Precision.

#pragma once

#include <cstdint>
#include <deque>
#include <vector>

namespace Animation
{
	class WeaponTrail
	{
	public:
		WeaponTrail() = default;

		// Apaga el efecto al destruirse (si no, se queda quieto hasta agotar su vida).
		~WeaponTrail();

		// Solo movible: cada instancia posee un único efecto.
		WeaponTrail(const WeaponTrail&) = delete;
		WeaponTrail& operator=(const WeaponTrail&) = delete;
		WeaponTrail(WeaponTrail&&) = default;
		WeaponTrail& operator=(WeaponTrail&&) = default;

		// Crea la estela en a_cell. a_upReference fija el plano, a_roll el giro sobre el avance y
		// a_anchorWorldOffset desplaza el anclaje; fijos en todo el tramo.
		void Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset);

		// Cambia el roll de los próximos segmentos.
		// Lo usa Return::BeginReturnMovement durante el enderezado final.
		void SetRoll(float a_roll);

		// Añade a_currentPosition al historial y recoloca los segmentos según la distancia recorrida.
		void Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds);

	private:
		RE::NiPointer<RE::BSTempEffectParticle> particle;
		std::vector<RE::NiPoint3>               history;

		// Huesos de Constants::kTrailRootNodeName ordenados por nombre ("Bone001"...).
		std::vector<RE::NiPointer<RE::NiAVObject>> orderedSegments;

		// Plano y roll fijados en Start.
		RE::NiPoint3 upReference{ 0.0f, 0.0f, 1.0f };
		float        roll{ 0.0f };
		RE::NiPoint3 anchorWorldOffset{ 0.0f, 0.0f, 0.0f };

		// Distancia acumulada y distancia de colocación de cada segmento (reciclado a kTrailLength).
		std::deque<float> segmentDistances;
		float             totalDistance{ 0.0f };

		std::uint32_t currentBoneIdx{ 0 };
		float         segmentsToAddRemainder{ 0.0f };
	};
}
