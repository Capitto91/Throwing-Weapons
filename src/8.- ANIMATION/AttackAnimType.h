// Cambia el tipo de arma a maza (weaponData.animationType) mientras está desenvainada y en reposo,
// para que OAR elija ataques de maza; envainada vuelve al tipo del registro (hacha).

#pragma once

namespace Animation::AttackAnimType
{
	// Registra el sink en los grafos de a_actor y sincroniza el tipo.
	// Lo llama EventManager al equipar, al cargar partida y tras la pantalla de carga.
	void EnsureRegistered(RE::Actor& a_actor);

	// Devuelve a_weapon a su tipo original. Lo llama EventManager al desequiparla.
	void Restore(RE::TESObjectWEAP* a_weapon);
}
