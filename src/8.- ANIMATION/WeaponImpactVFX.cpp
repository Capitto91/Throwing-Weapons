// Implementación del VFX de impacto. Ver el header para el porqué del
// pivote a una explosión vanilla real en vez de un .nif propio.

#include "8.- ANIMATION/WeaponImpactVFX.h"

#include "1.- CORE/Constants.h"

namespace Animation
{
	namespace
	{
		// Formulario resuelto una sola vez por sesión -- mismo patrón que
		// GetOnActivatorForm (WeaponVFX.cpp)/GetGlowActivatorForm
		// (WeaponGlow.cpp), aplicado aquí a nuestro propio BGSExplosion
		// (duplicado del vanilla en la Creation Kit, ver Constants.h) en
		// vez de un Activator.
		RE::BGSExplosion* GetImpactExplosionForm()
		{
			static RE::BGSExplosion* cache = nullptr;
			static bool              lookupDone = false;
			if (!lookupDone) {
				lookupDone = true;
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					cache = dataHandler->LookupForm<RE::BGSExplosion>(Constants::kImpactExplosionLocalFormID, Constants::kSoundPluginName);
				}
				if (!cache) {
					logs::warn("Animation::WeaponImpactVFX: no se encontró el BGSExplosion (FormID local 0x{:03X}) en \"{}\".",
						Constants::kImpactExplosionLocalFormID, Constants::kSoundPluginName);
				}
			}
			return cache;
		}
	}

	void SpawnImpactVFX(RE::TESObjectREFR& a_spawnAt, const RE::NiPoint3& a_position)
	{
		auto* form = GetImpactExplosionForm();
		if (!form) {
			return;
		}

		auto ref = a_spawnAt.PlaceObjectAtMe(form, false);
		if (!ref) {
			logs::warn("Animation::SpawnImpactVFX: PlaceObjectAtMe devolvió nullptr.");
			return;
		}

		ref->SetPosition(a_position);
	}
}
