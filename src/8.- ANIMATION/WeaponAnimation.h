// Giro, temblor y enderezado de la réplica por código, Globals de OAR y ocultar el arma equipada.

#pragma once

// El giro se escribe cada tick en local.rotate del nodo Constants::kWeaponSpinNodeName.

namespace Animation
{
	// Giro de a_refr compuesto sobre a_baseLocal, con rampa hasta Constants::kSpinAngularSpeed.
	// Lo llaman los bucles de Throw y Return antes de Physics::SyncHavok.
	void TickSpin(RE::TESObjectREFR& a_refr, float a_elapsedSeconds, const RE::NiMatrix3& a_baseLocal);

	// Rotación local actual del nodo de giro (identidad si no existe).
	RE::NiMatrix3 GetSpinLocalRotation(RE::TESObjectREFR& a_refr);

	// Funde la rotación del nodo de giro de a_blendFromLocal a a_targetLocal según a_blend (0..1).
	// La usa Return para enderezar el arma antes de llegar a la mano.
	void TickSpinStraighten(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_blendFromLocal, const RE::NiMatrix3& a_targetLocal, float a_blend);

	// Rotación mundial de la malla del arma equipada (hijos de "WEAPON").
	// La usa Throw::LaunchWeapon como base del giro.
	RE::NiMatrix3 GetEquippedWeaponWorldRotation(RE::Actor& a_actor);

	// Rotación mundial del hueso "WEAPON"; existe aunque no haya arma equipada.
	// La usa Return para orientar la llegada a la mano.
	RE::NiMatrix3 GetHandBoneWorldRotation(RE::Actor& a_actor);

	// Temblor de desprendimiento sobre a_baseRotation, con frecuencia y amplitud crecientes
	// durante a_duration. Lo llama Return::BeginReturn si el arma estaba clavada.
	void TickShudder(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_baseRotation, float a_elapsedSeconds, float a_duration);

	// Activa o desactiva el Global de Lanzar (Forms::throwTriggerGlobal) que lee OAR.
	void SetThrowTrigger(RE::Actor& a_actor, bool a_active);

	// Igual para Llamada (Forms::callTriggerGlobal).
	void SetCallTrigger(RE::Actor& a_actor, bool a_active);

	// Igual para Atrape (Forms::catchTriggerGlobal).
	void SetCatchTrigger(RE::Actor& a_actor, bool a_active);

	// Igual para el golpe en salto de Lightning Dash (Forms::slamTriggerGlobal).
	// false si el Global no existe (LightningDash baja entonces sin animación).
	bool SetSlamTrigger(RE::Actor& a_actor, bool a_active);

	// Activa o desactiva la graph variable vanilla Constants::kAnimationDrivenGraphVariable.
	void SetAnimationDriven(RE::Actor& a_actor, bool a_active);

	// Oculta o muestra la malla del arma equipada sin desequiparla, en los esqueletos de 1ª y 3ª persona. false si aún no tiene 3D.
	// La usa WeaponManager::ThrowWeapon al soltar el arma.
	bool SetEquippedWeaponHidden(RE::Actor& a_actor, bool a_hidden);

	// Valor de iRightHandType para a_weapon: su tipo si es de una mano (espada 1 a maza 4, la misma numeración del
	// grafo); si no, Constants::kRightHandTypeOneHanded. Lo usan Llamada, Atrape y el golpe en salto.
	[[nodiscard]] std::int32_t GetRightHandTypeFor(const RE::TESBoundObject* a_weapon);
}
