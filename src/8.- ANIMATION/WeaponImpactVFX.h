// Explosiones propias (BGSExplosion) del arma: impacto de la ida y golpe en salto. El motor las reproduce y las borra.

#pragma once

namespace Animation
{
	// Coloca la explosión de impacto en a_position; a_spawnAt solo aporta la celda.
	// La llama Throw::LaunchWeapon al impactar.
	void SpawnImpactVFX(RE::TESObjectREFR& a_spawnAt, const RE::NiPoint3& a_position);

	// Coloca la explosión del golpe en salto en a_position, siempre (sin consultar [Damage] ImpactExplosion) y con
	// a_owner como propietario, para que no le alcance. La llama LightningDash al tocar el suelo con la anotación del golpe.
	void SpawnSlamVFX(RE::Actor& a_owner, const RE::NiPoint3& a_position);
}
