// Formularios del plugin -- ver Forms.h.

#include "1.- CORE/Forms.h"

#include "1.- CORE/Constants.h"

namespace Forms
{
	namespace
	{
		// Formularios encontrados y sin encontrar en Load.
		int g_found = 0;
		int g_missing = 0;

		// Busca a_out por su FormID local en a_plugin; si falta, lo avisa en el log.
		template <class T>
		void Resolve(RE::TESDataHandler& a_dataHandler, T*& a_out, std::string_view a_plugin, RE::FormID a_localFormID, std::string_view a_description)
		{
			a_out = a_dataHandler.LookupForm<T>(a_localFormID, a_plugin);
			if (a_out) {
				++g_found;
				return;
			}

			++g_missing;
			logs::warn("Forms: no se encontró {} (FormID local 0x{:03X}) en \"{}\".", a_description, a_localFormID, a_plugin);
		}
	}

	void Load()
	{
		g_found = 0;
		g_missing = 0;

		auto* dataHandler = RE::TESDataHandler::GetSingleton();
		if (!dataHandler) {
			logs::error("Forms::Load: sin TESDataHandler, no se resuelve ningún formulario.");
			return;
		}
		auto& data = *dataHandler;

		// Sin el .esp del mod se avisa una sola vez; Requirements::CheckPluginFile lo marca como requisito que falta.
		using Constants::kPluginName;
		if (dataHandler->LookupModByName(kPluginName)) {
			Resolve(data, throwableWeaponKeyword, kPluginName, Constants::kThrowableWeaponKeywordLocalFormID, "la keyword del arma arrojadiza");
			Resolve(data, throwTriggerGlobal, kPluginName, Constants::kThrowTriggerGlobalLocalFormID, "el Global de Lanzar");
			Resolve(data, callTriggerGlobal, kPluginName, Constants::kCallTriggerGlobalLocalFormID, "el Global de Llamada");
			Resolve(data, catchTriggerGlobal, kPluginName, Constants::kCatchTriggerGlobalLocalFormID, "el Global de Atrape");
			Resolve(data, slamTriggerGlobal, kPluginName, Constants::kSlamTriggerGlobalLocalFormID, "el Global del golpe en salto");
			Resolve(data, paralysisSpell, kPluginName, Constants::kEmbeddedParalysisSpellLocalFormID, "el hechizo de parálisis");
			Resolve(data, paralysisEffect, kPluginName, Constants::kEmbeddedParalysisEffectLocalFormID, "el efecto de parálisis");
			Resolve(data, actorHazard, kPluginName, Constants::kEmbeddedHazardLocalFormID, "el hazard de actor");
			Resolve(data, surfaceHazard, kPluginName, Constants::kSurfaceHazardLocalFormID, "el hazard de superficie");
			Resolve(data, lightningDashSpell, kPluginName, Constants::kLightningDashSpellLocalFormID, "el poder Lightning Dash");
			Resolve(data, lightningDashCooldownSpell, kPluginName, Constants::kLightningDashCooldownSpellLocalFormID, "el hechizo de cooldown de Lightning Dash");
			Resolve(data, lightningDashCooldownEffect, kPluginName, Constants::kLightningDashCooldownEffectLocalFormID, "el efecto de cooldown de Lightning Dash");
			Resolve(data, lightningDashVfxSpell, kPluginName, Constants::kLightningDashVFXSpellLocalFormID, "el hechizo del aspecto de Lightning Dash");
			Resolve(data, lightningDashImageSpaceModifier, kPluginName, Constants::kLightningDashImageSpaceModLocalFormID, "el modificador de imagen de Lightning Dash");
			Resolve(data, lightningDashLegacyVisualEffect, kPluginName, Constants::kLightningDashVisualEffectLocalFormID, "el VisualEffect antiguo de Lightning Dash");
			Resolve(data, impactExplosion, kPluginName, Constants::kImpactExplosionLocalFormID, "la explosión de impacto");
			Resolve(data, slamExplosion, kPluginName, Constants::kSlamExplosionLocalFormID, "la explosión del golpe en salto");
			Resolve(data, weaponGlowActivator, kPluginName, Constants::kWeaponGlowActivatorLocalFormID, "el Activator del destello");
			Resolve(data, weaponGlowLight, kPluginName, Constants::kWeaponGlowLightLocalFormID, "la luz del destello");
			Resolve(data, handGlowArtObject, kPluginName, Constants::kHandGlowArtObjectLocalFormID, "el arte del brillo de manos");
		} else {
			logs::warn("Forms::Load: {} no está cargado, sus formularios quedan sin resolver.", kPluginName);
		}

		using Constants::kSkyrimPluginName;
		Resolve(data, lightningDashDustExplosion, kSkyrimPluginName, Constants::kLightningDashDustExplosionFormID, "la explosión de polvo de Lightning Dash");
		Resolve(data, lightningDashShockExplosion, kSkyrimPluginName, Constants::kLightningDashShockExplosionFormID, "la explosión eléctrica de Lightning Dash");
		Resolve(data, actorTypeDragon, kSkyrimPluginName, Constants::kGlowMapDragonKeywordFormID, "la keyword ActorTypeDragon");
		Resolve(data, actorTypeUndead, kSkyrimPluginName, Constants::kGlowMapUndeadKeywordFormID, "la keyword ActorTypeUndead");
		Resolve(data, actorTypeDaedra, kSkyrimPluginName, Constants::kGlowMapDaedraKeywordFormID, "la keyword ActorTypeDaedra");

		if (g_missing == 0) {
			logs::info("Forms::Load: {} formularios resueltos.", g_found);
		} else {
			logs::warn("Forms::Load: {} formularios resueltos y {} sin encontrar (avisos de arriba).", g_found, g_missing);
		}
	}
}
