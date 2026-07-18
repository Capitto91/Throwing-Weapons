// Estela visual del arma durante el vuelo (ida y vuelta, ver PLAN-trail.md).
// Reposiciona cada tick los segmentos ya modelados de un NIF de efecto
// (Constants::kTrailEffectPath) siguiendo el historial reciente de
// posiciones de la réplica en vuelo.
//
// Basado en el sistema de estelas de Precision (Ershin, MIT License,
// github.com/ersh1/Precision, src/AttackTrail.h/.cpp) -- reimplementado
// para seguir la posición de la réplica en vuelo en vez de un arma
// equipada en la mano de un actor, así que no necesita nada de la lógica
// de "qué NIF de estela usar según el arma/encantamiento" del original:
// siempre es la misma réplica única.

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

		// Fuerza al efecto a expirar de inmediato si seguía activo (ver
		// Constants abajo y la implementación) -- sin esto, el NIF
		// spawneado se queda visible y quieto en el sitio hasta agotar su
		// propio tiempo de vida (varios segundos), tanto al clavarse como
		// al cancelarse el vuelo desde fuera (botón de recuperar a mitad
		// de la ida). Comprobado en el juego.
		~WeaponTrail();

		// Solo movible: cada instancia posee un único efecto en la
		// escena. Copiarla duplicaría el NiPointer sin duplicar el efecto,
		// y la primera copia en destruirse lo apagaría para la otra.
		WeaponTrail(const WeaponTrail&) = delete;
		WeaponTrail& operator=(const WeaponTrail&) = delete;
		WeaponTrail(WeaponTrail&&) = default;
		WeaponTrail& operator=(WeaponTrail&&) = default;

		// Crea el efecto de estela (Constants::kTrailEffectPath) en
		// a_cell, posicionado en a_initialTransform. Sin efecto (con
		// aviso en el log) si el NIF no llega a cargar -- las siguientes
		// llamadas a Update simplemente no harán nada.
		void Start(RE::TESObjectCELL* a_cell, const RE::NiTransform& a_initialTransform);

		// Añade a_currentTransform al historial y reposiciona los
		// segmentos de la cadena Constants::kTrailRootNodeName según el
		// tiempo transcurrido desde el tick anterior. Sin efecto si Start
		// no llegó a crear el efecto, o si su NIF no tiene esa cadena de
		// segmentos.
		void Update(const RE::NiTransform& a_currentTransform, float a_deltaSeconds);

	private:
		RE::NiPointer<RE::BSTempEffectParticle> particle;
		std::vector<RE::NiTransform>            history;
		std::deque<float>                       segmentTimestamps;
		std::uint32_t                           currentBoneIdx{ 0 };
		float                                   currentTime{ 0.0f };
		float                                   segmentsToAddRemainder{ 0.0f };
		bool                                    appliedColorSettings{ false };
	};
}
