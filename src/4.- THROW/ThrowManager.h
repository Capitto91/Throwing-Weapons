// Gestiona todo el proceso de lanzamiento.
// Crea el proyectil réplica del arma, calcula posición inicial y aplica la fuerza
// inicial del lanzamiento.

#pragma once

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
}