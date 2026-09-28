// Puente opcional a True Directional Movement: con un objetivo fijado, Lanzar apunta a él.
// Sin TDM o sin lock no hace nada.

#pragma once

namespace TDMBridge
{
	// Pide la API de TDM. Lo llama EventManager en kPostLoad; sin TDM solo avisa por log.
	void Init();

	// Actor fijado por TDM, o nullptr. Hilo principal.
	[[nodiscard]] RE::NiPointer<RE::Actor> GetLockedTarget();

	// Punto del cuerpo al que apuntar (hueso de torso de la raza, o GetLookingAtLocation).
	// Lo usa Throw::LaunchWeapon.
	[[nodiscard]] RE::NiPoint3 GetTargetPoint(RE::Actor& a_target);

	// Ajusta a_projectileVelocity para interceptar a un objetivo con a_targetVelocity y gravedad a_gravity.
	// Copia de PredictAimProjectile de TDM; false si no hay solución exacta.
	bool PredictAimProjectile(const RE::NiPoint3& a_projectilePos, const RE::NiPoint3& a_targetPosition, const RE::NiPoint3& a_targetVelocity, float a_gravity, RE::NiPoint3& a_projectileVelocity);
}
