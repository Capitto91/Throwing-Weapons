// Chispas que siguen al arma mientras se mueve: NIF creado con BSTempEffectParticle, con su nodo emisor
// movido cada tick; al apagarse deja de emitir y se retira cuando mueren sus partículas.

#pragma once

namespace Animation
{
	// Enciende las chispas siguiendo el hueso "WEAPON" de a_actor; si ya están encendidas, solo cambian de objetivo.
	// Lo llaman WeaponManager::TransitionState y PowerAttackVFX (a_checkSetting=false).
	void StartMovementVFXOnActor(RE::Actor& a_actor, bool a_checkSetting = true);

	// Enciende las chispas siguiendo la réplica a_handle (con 3D ya cargado).
	// Lo llama WeaponManager al pasar a kThrown/kCalling/kReturning.
	void StartMovementVFXOnReplica(RE::ObjectRefHandle a_handle);

	// Las chispas activas pasan a seguir el hueso "WEAPON" de a_actor, sin crear otro efecto.
	// Lo llama WeaponManager::ReequipAndReset durante el Atrape.
	void RetargetMovementVFXToActor(RE::Actor& a_actor);

	// Corta las chispas de golpe.
	void StopMovementVFX();

	// Apaga las chispas: deja de emitir y retira el efecto cuando mueren sus partículas.
	// Con a_extraSettleDelay espera Constants::kCatchVfxSettleDelay (FinishCatchAnimation).
	void FadeOutMovementVFX(bool a_extraSettleDelay = false);
}
