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
}
