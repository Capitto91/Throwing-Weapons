// Estela visual del arma durante el vuelo (ida y vuelta).
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
//
// Historia: hubo una primera versión de esto (v1.7.1) que se abandonó por
// completo en v1.9.8 ("a petición del usuario tras varias rondas de
// depuración sin resultado convincente", ver CHANGELOG.md) -- la
// arquitectura de fiabilidad (spawn único, reposicionar huesos ya
// existentes, tangente por historial de posiciones, parada por RAII) ya
// funcionaba correctamente en ese momento; el motivo real del abandono
// fue casi con toda seguridad estético (malla genérica de espada de
// Precision con un tinte azul plano, sin ninguna textura de rayo). Esta
// versión recupera esa misma arquitectura tal cual (código real
// recuperado de git, commit 43d7b1c) y solo simplifica dos cosas que ya
// no hacen falta: el punto de anclaje ya no se desplaza en el espacio del
// giro (Constants::kTrailAnchorLocalOffset, eliminado -- usa
// directamente la posición del nodo raíz de la réplica), y el color/
// brillo ya no se sobreescribe por código (kTrailBaseColor/
// kTrailBaseColorScaleMult, eliminados -- se hornea en el shader del NIF
// final, mismo mecanismo que ThorMjolnirSparks.nif).

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

		// Fuerza al efecto a expirar de inmediato si seguía activo -- sin
		// esto, el NIF spawneado se queda visible y quieto en el sitio
		// hasta agotar su propio tiempo de vida (varios segundos), tanto
		// al clavarse como al cancelarse el vuelo desde fuera (botón de
		// recuperar a mitad de la ida). Comprobado en el juego (versión
		// anterior, v1.7.1).
		~WeaponTrail();

		// Solo movible: cada instancia posee un único efecto en la
		// escena. Copiarla duplicaría el NiPointer sin duplicar el
		// efecto, y la primera copia en destruirse lo apagaría para la
		// otra.
		WeaponTrail(const WeaponTrail&) = delete;
		WeaponTrail& operator=(const WeaponTrail&) = delete;
		WeaponTrail(WeaponTrail&&) = default;
		WeaponTrail& operator=(WeaponTrail&&) = default;

		// Crea el efecto de estela (Constants::kTrailEffectPath) en
		// a_cell, posicionado en a_initialPosition. Sin efecto (con aviso
		// en el log) si el NIF no llega a cargar -- las siguientes
		// llamadas a Update simplemente no harán nada.
		void Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition);

		// Añade a_currentPosition al historial y reposiciona los
		// segmentos de la cadena Constants::kTrailRootNodeName según el
		// tiempo transcurrido desde el tick anterior. Sin efecto si Start
		// no llegó a crear el efecto, o si su NIF no tiene esa cadena de
		// segmentos.
		void Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds);

	private:
		RE::NiPointer<RE::BSTempEffectParticle> particle;
		std::vector<RE::NiPoint3>               history;
		std::deque<float>                       segmentTimestamps;
		std::uint32_t                           currentBoneIdx{ 0 };
		float                                   currentTime{ 0.0f };
		float                                   segmentsToAddRemainder{ 0.0f };
	};
}
