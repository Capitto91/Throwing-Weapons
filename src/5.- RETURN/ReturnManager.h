// Controla el regreso del arma hacia el jugador.
// Gestiona inicio del retorno, seguimiento de trayectoria y recuperación final.

#pragma once

namespace Return
{
	// Empieza a controlar a mano, tick a tick, el proyectil identificado
	// por a_handle (ya existente, en vuelo o clavado, con el
	// ProjectileHandle correspondiente ya persistido en WeaponState por
	// Weapon::WeaponManager) hasta que llega a la mano de a_player. Pone
	// el proyectil en modo Havok "keyframed" (movido por código, sin
	// fuerzas/gravedad) antes de empezar a moverlo — ver CLAUDE.md,
	// "Arquitectura de física de proyectiles". Al llegar, destruye el
	// proyectil y notifica a Weapon::WeaponManager::OnReturnComplete.
	// Fase 1: trayectoria en línea recta a velocidad constante, sin
	// curvatura/velocidad híbrida/homing/enderezado/temblor todavía
	// (puntos 7, 8, 9, 10, 11, 12 de Mecanica del arma.txt pendientes).
	void BeginReturn(RE::Actor* a_player, RE::ProjectileHandle a_handle);
}
