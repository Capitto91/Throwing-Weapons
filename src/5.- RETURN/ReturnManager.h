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
	// Trayectoria curva (nunca en línea recta, punto 7) con aceleración
	// híbrida partiendo de velocidad cero (punto 8). Pendiente: golpear
	// sin clavarse durante el regreso (punto 9), homing con ángulo máximo
	// (punto 10, descartado tras pruebas en el juego — ver CHANGELOG.md),
	// enderezado antes de llegar (punto 11) y temblor previo al
	// desprendimiento (punto 12).
	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_handle);
}
