// Explosión de impacto -- ver WeaponImpactVFX.h.

#include "8.- ANIMATION/WeaponImpactVFX.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Settings.h"

namespace Animation
{
	namespace
	{
		// Formulario resuelto una vez por sesión.
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
		// Desactivable desde [Damage] ImpactExplosion (Settings / menú).
		if (!Settings::GetImpactExplosion()) {
			return;
		}

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
