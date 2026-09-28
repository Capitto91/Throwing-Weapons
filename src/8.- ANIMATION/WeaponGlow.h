// Destello con luz (ThorMjolnirLight.nif + TESObjectLIGH) que sigue a la mano o a la réplica
// desde Lanzar hasta el final del Atrape; un solo Activator por ciclo.

#pragma once

namespace Animation
{
	// Posición mundial de la cabeza del martillo (nodo "Gold" + offset) bajo a_root.
	// La usan el destello y Animation::SpawnImpactVFX.
	RE::NiPoint3 GetGlowAnchorPosition(RE::NiAVObject* a_root);

	// Coloca el destello siguiendo el hueso "WEAPON" de a_actor. true si colocó uno nuevo.
	// Lo llaman WeaponManager::BeginThrowAnimation y PowerAttackVFX (a_checkSetting=false).
	bool StartWeaponGlow(RE::Actor& a_actor, bool a_checkSetting = true);

	// Pasa a seguir la réplica a_handle; si desaparece, se queda en su última posición.
	// Lo llama WeaponManager::ThrowWeapon al crearse la réplica.
	void RetargetWeaponGlowToReplica(RE::ObjectRefHandle a_handle);

	// Vuelve a seguir el hueso "WEAPON" de a_actor.
	// Lo llama WeaponManager::ReequipAndReset durante el Atrape.
	void RetargetWeaponGlowToActor(RE::Actor& a_actor);

	// Apaga el destello con fundido y lo borra.
	// Lo llaman WeaponManager al terminar el Atrape o al recuperar sin animación, y PowerAttackVFX.
	void StopWeaponGlow();
}
