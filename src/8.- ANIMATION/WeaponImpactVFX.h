// Explosión (BGSExplosion propio) en cada impacto del arma; el motor la reproduce y la borra.

#pragma once

namespace Animation
{
	// Coloca la explosión en a_position; a_spawnAt solo aporta la celda.
	// La llama Throw::LaunchWeapon al impactar.
	void SpawnImpactVFX(RE::TESObjectREFR& a_spawnAt, const RE::NiPoint3& a_position);
}
