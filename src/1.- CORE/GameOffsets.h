// Funciones nativas del motor no expuestas por commonlibsse-ng.
// Declaradas a mano via REL::Relocation con IDs de Address Library
// verificados -- nunca direcciones fijas ni funciones inventadas (ver
// CLAUDE.md, "Errores comunes a vigilar"). Cada funcion documenta de
// donde sale su ID y su fuente de verificacion.

#pragma once

namespace GameOffsets
{
	// Funcion interna real del pipeline de combate cuerpo a cuerpo del
	// motor: aplica el golpe del arma actualmente equipada por
	// a_attacker sobre a_target (dano, reacciones de combate, aggro,
	// crimen si aplica). a_sourceProjectile es el RE::Projectile de
	// origen si el golpe viene de un proyectil nativo (no lo usamos,
	// siempre nullptr); a_bLeftHand indica si el golpe viene de la mano
	// izquierda.
	//
	// ID localizado en el codigo fuente publico de Precision (Ershin,
	// https://github.com/ersh1/Precision, src/Offsets.h), donde se usa
	// con este mismo proposito (que sus golpes de colision precisa
	// generen la misma reaccion de combate/crimen que un golpe vanilla)
	// -- no es una direccion inventada, es un ID de Address Library ya
	// verificado y estable en un plugin publico muy usado.
	//
	// Solo tiene ID verificado para SE/AE (Precision no soporta VR).
	// Comprobar REL::Module::IsVR() antes de llamar -- no hay variante
	// VR conocida, y REL::RelocationID con solo dos argumentos reutiliza
	// silenciosamente el ID de SE como si fuera el de VR, lo que
	// resolveria a una direccion incorrecta.
	using tDealDamage = void*(__fastcall*)(RE::Actor* a_attacker, RE::Actor* a_target, RE::Projectile* a_sourceProjectile, bool a_bLeftHand);
	inline REL::Relocation<tDealDamage> DealDamage{ REL::RelocationID(37673, 38627) };

	// Nombre real de la funcion de arriba: Actor::CombatHit (comentario
	// "140628dd7 Actor::CombatHit" en fenix31415/NewProjectilesTMP,
	// src/Triggers.cpp, que engancha el mismo ID). Por dentro hace dos
	// pasos: (1) HitData::Populate con el arma EQUIPADA del atacante (ID
	// 42832/44001, ya expuesto en commonlibsse-ng) y (2) la funcion que
	// procesa el golpe de verdad, void(Actor* victima, HitData&): dano con
	// dificultad, reacciones de golpe, aviso a IA de combate/crimen,
	// muerte con autoria, TESHitEvent. Para golpear con el arma lanzada
	// (la mano va vacia) se hace (1) a mano con la entrada del arma y se
	// llama a (2) directamente -- mismo esquema que KratosCombat (su .pdb
	// contiene una REL::Relocation<void(Actor*, HitData&)> junto a
	// HitData::Populate).
	//
	// No hay ID de Address Library verificado para (2). Se obtiene leyendo
	// la instruccion call (E8 rel32) que lo invoca dentro de CombatHit, en
	// +0x3C0 (SE) / +0x4A8 (AE): desplazamientos verificados en
	// D7ry/valhallaCombat (src/include/Hooks.h, Hook_OnMeleeHit, que
	// engancha ese mismo call con write_call<5>). Se comprueba el opcode
	// antes de fiarse: si no es E8 (version del juego distinta, u otro mod
	// que haya parcheado el sitio de otra forma), devuelve nullptr y el
	// llamante cae al sistema anterior. Si otro mod ya engancho ese call
	// con un trampolin (Valhalla Combat, EldenParry...), el destino es su
	// gancho, que acaba llamando al original: nuestro golpe pasa por su
	// logica (bloqueo, parry) como cualquier golpe cuerpo a cuerpo.
	//
	// Sin desplazamiento verificado en VR: devuelve nullptr.
	using tProcessHit = void (*)(RE::Actor* a_victim, RE::HitData& a_hitData);

	inline tProcessHit ResolveProcessHit()
	{
		if (REL::Module::IsVR()) {
			return nullptr;
		}

		const REL::Relocation<std::uintptr_t> callSite{ REL::RelocationID(37673, 38627), REL::VariantOffset(0x3C0, 0x4A8, 0) };
		const auto*                           bytes = reinterpret_cast<const std::uint8_t*>(callSite.address());
		if (bytes[0] != 0xE8) {
			return nullptr;
		}

		std::int32_t rel32 = 0;
		std::memcpy(&rel32, bytes + 1, sizeof(rel32));
		return reinterpret_cast<tProcessHit>(callSite.address() + 5 + rel32);
	}
}
