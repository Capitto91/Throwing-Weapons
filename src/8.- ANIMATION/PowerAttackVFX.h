// Chispas y destello con luz durante cada power attack con el martillo en la mano.
// Escucha weaponSwing/attackStop del grafo del jugador y enciende WeaponVFX y WeaponGlow.

#pragma once

namespace RE
{
	class Actor;
}

namespace Animation::PowerAttackVFX
{
	// Registra el sink en los grafos de a_actor (idempotente).
	// Lo llama EventManager al equipar, al cargar partida y al cerrar la pantalla de carga.
	void EnsureRegistered(RE::Actor& a_actor);

	// Da por terminados los efectos en curso y apaga el destello.
	// Lo llama WeaponManager::BeginThrowAnimation antes de encender los del lanzamiento.
	void Cancel();
}
