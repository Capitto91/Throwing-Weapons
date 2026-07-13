// Gestiona todo el proceso de lanzamiento.
// Crea el proyectil réplica del arma, calcula posición inicial y aplica la fuerza
// inicial del lanzamiento.

#pragma once

#include <optional>

namespace Throw
{
	// Lanza el proyectil réplica del arma desde la mano derecha del actor,
	// hacia donde apunta la cámara. No hay mecánica de carga (Mecanica del
	// arma.txt, punto 3): la fuerza del lanzamiento la determina por
	// completo el formulario Projectile configurado en la Creation Kit, no
	// algo que calculemos aquí. a_weapon es el arma que se está lanzando
	// (para asociarla al proyectil, vía Projectile::LaunchArrow). Devuelve
	// el handle del proyectil creado (vacío si el lanzamiento falla), para
	// que el llamante pueda seguirlo (ver Throw::TrackProjectile) y
	// persistirlo en WeaponState.
	[[nodiscard]] RE::ProjectileHandle LaunchWeapon(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon);

	// Crea el proyectil réplica ya en reposo en a_position, sin lanzarlo
	// (ángulos a cero); usado por Weapon::WeaponManager para arrancar el
	// regreso (5.- RETURN) cuando no hay ya un Projectile en vuelo/clavado
	// del que partir (caso "clavado en un actor", ver
	// Throw::DetachEmbeddedWeapon). El llamante debe ponerlo en modo
	// Havok "keyframed" antes de que la física le dé tiempo a moverlo.
	[[nodiscard]] RE::ProjectileHandle SpawnProjectileAt(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon, const RE::NiPoint3& a_position);

	// Al clavarse en un actor, la referencia Projectile original deja de
	// existir (comprobado en el juego: el motor la destruye al procesar
	// el golpe) y el modelo del arma queda enganchado directamente al
	// árbol de nodos 3D de a_target, con un retraso de aprox. 1 segundo
	// tras el impacto (ver Constants::kEmbeddedWeaponNodeName). Busca ese
	// nodo recursivamente y lo desengancha si lo encuentra, devolviendo su
	// posición en el mundo (vacío si a_target es nulo o el nodo todavía no
	// existe, p. ej. si se llama menos de un segundo después del
	// impacto).
	[[nodiscard]] std::optional<RE::NiPoint3> DetachEmbeddedWeapon(RE::TESObjectREFR* a_target);
}