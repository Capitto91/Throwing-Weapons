// Destello con luz (clon de ThorMjolnirLight.nif + NiPointLight con los datos del TESObjectLIGH) colgado de la mano o de
// la réplica desde Lanzar hasta el final del Atrape; uno activo a la vez.

#pragma once

namespace Animation
{
	// Posición mundial de la cabeza del martillo (nodo "Gold" + offset) bajo a_root.
	// La usan el destello y Animation::SpawnImpactVFX.
	RE::NiPoint3 GetGlowAnchorPosition(RE::NiAVObject* a_root);

	// true si a_object es el nodo raíz de un destello. Lo usa WeaponAnimation para saltárselo entre los hijos del hueso
	// "WEAPON".
	bool IsWeaponGlowNode(const RE::NiAVObject* a_object);

	// Cuelga un destello del hueso "WEAPON" de a_actor. true si creó uno nuevo.
	// Lo llaman WeaponManager::BeginThrowAnimation y PowerAttackVFX (a_checkSetting=false).
	bool StartWeaponGlow(RE::Actor& a_actor, bool a_checkSetting = true);

	// Pasa a colgar de la réplica a_handle; si desaparece, se oculta.
	// Lo llama WeaponManager::ThrowWeapon al crearse la réplica.
	void RetargetWeaponGlowToReplica(RE::ObjectRefHandle a_handle);

	// Vuelve a colgar del hueso "WEAPON" de a_actor.
	// Lo llama WeaponManager::ReequipAndReset durante el Atrape.
	void RetargetWeaponGlowToActor(RE::Actor& a_actor);

	// Apaga el destello con fundido; al terminar se retira de la escena.
	// Lo llaman WeaponManager al terminar el Atrape o al recuperar sin animación, y PowerAttackVFX.
	void StopWeaponGlow();

	// Retira el destello sin fundido. Lo llama WeaponManager::ResetToInHand al cargar partida.
	void StopWeaponGlowNow();

	// Borra de las celdas cargadas los Activator del destello guardados en la partida (ni el .esp ni el plugin colocan
	// ninguno). Lo llama EventManager al cerrarse cada pantalla de carga.
	void RemoveStrayWeaponGlows();
}
