// Formularios que usa el plugin, resueltos una vez al cargar los datos del juego (como las propiedades de un script
// en la Creation Kit). nullptr si falta: Load lo avisa en el log y quien lo usa se queda sin ese efecto.

#pragma once

namespace Forms
{
	// -- ThorMjolnirOAR.esp (Constants::kPluginName) --

	// Keyword que identifica al arma arrojadiza.
	inline RE::BGSKeyword* throwableWeaponKeyword{ nullptr };

	// Globals que leen los submods de OAR para sustituir el ataque ligero por Lanzar, Llamada, Atrape y el golpe en salto.
	inline RE::TESGlobal* throwTriggerGlobal{ nullptr };
	inline RE::TESGlobal* callTriggerGlobal{ nullptr };
	inline RE::TESGlobal* catchTriggerGlobal{ nullptr };
	inline RE::TESGlobal* slamTriggerGlobal{ nullptr };

	// Parálisis mientras el arma está clavada en un actor y su efecto (para detectar inmunidad).
	inline RE::SpellItem*     paralysisSpell{ nullptr };
	inline RE::EffectSetting* paralysisEffect{ nullptr };

	// Descargas eléctricas al clavarse en un actor y en una superficie.
	inline RE::BGSHazard* actorHazard{ nullptr };
	inline RE::BGSHazard* surfaceHazard{ nullptr };

	// Lightning Dash: el poder, su cooldown (hechizo y efecto), el aspecto del desplazamiento, el modificador de imagen
	// y el VisualEffect antiguo cuyos efectos guardados se retiran al cargar.
	inline RE::SpellItem*             lightningDashSpell{ nullptr };
	inline RE::SpellItem*             lightningDashCooldownSpell{ nullptr };
	inline RE::EffectSetting*         lightningDashCooldownEffect{ nullptr };
	inline RE::SpellItem*             lightningDashVfxSpell{ nullptr };
	inline RE::TESImageSpaceModifier* lightningDashImageSpaceModifier{ nullptr };
	inline RE::BGSReferenceEffect*    lightningDashLegacyVisualEffect{ nullptr };

	// Explosiones de cada impacto de la ida y del golpe en salto.
	inline RE::BGSExplosion* impactExplosion{ nullptr };
	inline RE::BGSExplosion* slamExplosion{ nullptr };

	// Destello del martillo (Activator con ThorMjolnirLight.nif y su luz) y arte del brillo de manos.
	inline RE::TESObjectACTI* weaponGlowActivator{ nullptr };
	inline RE::TESObjectLIGH* weaponGlowLight{ nullptr };
	inline RE::BGSArtObject*  handGlowArtObject{ nullptr };

	// -- Skyrim.esm (Constants::kSkyrimPluginName) --

	// Explosiones de polvo y descarga al empezar Lightning Dash.
	inline RE::BGSExplosion* lightningDashDustExplosion{ nullptr };
	inline RE::BGSExplosion* lightningDashShockExplosion{ nullptr };

	// Keywords de raza de los tipos de criatura que encienden el glow del martillo (GlowMapControl).
	inline RE::BGSKeyword* actorTypeDragon{ nullptr };
	inline RE::BGSKeyword* actorTypeUndead{ nullptr };
	inline RE::BGSKeyword* actorTypeDaedra{ nullptr };

	// Ranura de equipado de la mano derecha: con el ciclo en marcha se desequipa solo de ella (EventManager).
	inline RE::BGSEquipSlot* rightHandEquipSlot{ nullptr };

	// Busca todos los formularios y avisa en el log de cada uno que falte. Lo llama EventManager en kDataLoaded,
	// antes que nada que los use.
	void Load();
}
