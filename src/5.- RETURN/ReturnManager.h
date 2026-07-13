// Controla el regreso del arma hacia el jugador.
// Gestiona inicio del retorno, seguimiento de trayectoria y recuperación final.

#pragma once

namespace Return
{
	// Empieza a controlar a mano, tick a tick, la réplica visual del arma
	// identificada por a_handle (creada por Throw::SpawnWeaponReplicaAt,
	// no un Projectile — ver el porqué en ese comentario) hasta que llega
	// a la mano de a_player. La pone en modo Havok "keyframed" (movido por
	// código, sin fuerzas/gravedad) antes de empezar a moverla — ver
	// CLAUDE.md, "Arquitectura de física de proyectiles". Al llegar, la
	// borra y notifica a Weapon::WeaponManager::OnReturnComplete.
	// Fase 1: trayectoria en línea recta a velocidad constante, sin
	// curvatura/velocidad híbrida/homing/enderezado/temblor todavía
	// (puntos 7, 8, 9, 10, 11, 12 de Mecanica del arma.txt pendientes).
	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_handle);
}
