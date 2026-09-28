// Chispas que siguen al arma mientras se mueve (Activator colocado y movido cada tick).
// Apagado con un segundo Activator "de un solo uso" que se extingue solo.

#pragma once

namespace Animation
{
	// Enciende las chispas siguiendo el hueso "WEAPON" de a_actor, solapando con las anteriores.
	// Lo llaman WeaponManager::TransitionState y PowerAttackVFX (a_checkSetting=false).
	void StartMovementVFXOnActor(RE::Actor& a_actor, bool a_checkSetting = true);

	// Enciende las chispas siguiendo la réplica a_handle (con 3D ya cargado).
	// Lo llama WeaponManager al pasar a kThrown/kCalling/kReturning.
	void StartMovementVFXOnReplica(RE::ObjectRefHandle a_handle);

	// Las chispas activas pasan a seguir el hueso "WEAPON" de a_actor, sin recolocar nada.
	// Lo llama WeaponManager::ReequipAndReset durante el Atrape.
	void RetargetMovementVFXToActor(RE::Actor& a_actor);

	// Corta las chispas de golpe y borra sus referencias.
	void StopMovementVFX();

	// Apaga las chispas con el Activator "de un solo uso" en su posición; no-op si ya hay uno en marcha.
	// Con a_extraSettleDelay espera Constants::kCatchVfxSettleDelay (FinishCatchAnimation).
	void FadeOutMovementVFX(bool a_extraSettleDelay = false);
}
