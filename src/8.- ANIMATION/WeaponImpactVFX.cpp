// Explosiones del arma -- ver WeaponImpactVFX.h.

#include "8.- ANIMATION/WeaponImpactVFX.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Settings.h"

namespace Animation
{
	namespace
	{
		// BGSExplosion del ESL por FormID local; avisa si no está. Para inicializar las cachés de abajo una sola vez.
		RE::BGSExplosion* LookupExplosion(RE::FormID a_localFormID)
		{
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			auto* form = dataHandler ? dataHandler->LookupForm<RE::BGSExplosion>(a_localFormID, Constants::kSoundPluginName) : nullptr;
			if (!form) {
				logs::warn("Animation::WeaponImpactVFX: no se encontró el BGSExplosion (FormID local 0x{:03X}) en \"{}\".",
					a_localFormID, Constants::kSoundPluginName);
			}
			return form;
		}

		RE::BGSExplosion* GetImpactExplosionForm()
		{
			static RE::BGSExplosion* form = LookupExplosion(Constants::kImpactExplosionLocalFormID);
			return form;
		}

		RE::BGSExplosion* GetSlamExplosionForm()
		{
			static RE::BGSExplosion* form = LookupExplosion(Constants::kSlamExplosionLocalFormID);
			return form;
		}

		// Coloca a_form en a_position; con a_owner, propietario de la explosión antes de que el motor busque
		// a quién alcanza (en su primera actualización).
		void PlaceExplosion(RE::TESObjectREFR& a_spawnAt, RE::BGSExplosion* a_form, const RE::NiPoint3& a_position, RE::Actor* a_owner)
		{
			if (!a_form) {
				return;
			}

			auto ref = a_spawnAt.PlaceObjectAtMe(a_form, false);
			if (!ref) {
				logs::warn("Animation::WeaponImpactVFX: PlaceObjectAtMe devolvió nullptr.");
				return;
			}

			ref->SetPosition(a_position);

			if (a_owner) {
				if (auto* explosion = skyrim_cast<RE::Explosion*>(ref.get())) {
					explosion->GetExplosionRuntimeData().actorOwner = a_owner->GetHandle();
				} else {
					logs::warn("Animation::WeaponImpactVFX: la referencia colocada no es una Explosion, sin propietario.");
				}
			}
		}
	}

	void SpawnImpactVFX(RE::TESObjectREFR& a_spawnAt, const RE::NiPoint3& a_position)
	{
		// Desactivable desde [Damage] ImpactExplosion (Settings / menú).
		if (!Settings::GetImpactExplosion()) {
			return;
		}

		PlaceExplosion(a_spawnAt, GetImpactExplosionForm(), a_position, nullptr);
	}

	void SpawnSlamVFX(RE::Actor& a_owner, const RE::NiPoint3& a_position)
	{
		PlaceExplosion(a_owner, GetSlamExplosionForm(), a_position, &a_owner);
	}
}
