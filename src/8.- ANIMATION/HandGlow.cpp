// Implementación del destello de manos. Ver el header y Constants.h
// ("-- Brillo de manos --") para la arquitectura completa.

#include "8.- ANIMATION/HandGlow.h"

#include "1.- CORE/Constants.h"

namespace Animation
{
	namespace
	{
		// Formulario resuelto una sola vez por sesión -- mismo patrón que
		// GetGlowLightForm en WeaponGlow.cpp.
		RE::BGSArtObject* GetHandGlowArtObject()
		{
			static RE::BGSArtObject* cache = nullptr;
			static bool              lookupDone = false;
			if (!lookupDone) {
				lookupDone = true;
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					cache = dataHandler->LookupForm<RE::BGSArtObject>(Constants::kHandGlowArtObjectLocalFormID, Constants::kSoundPluginName);
				}
				if (!cache) {
					logs::warn("Animation::HandGlow: no se encontró el BGSArtObject (FormID local 0x{:03X}) en \"{}\".",
						Constants::kHandGlowArtObjectLocalFormID, Constants::kSoundPluginName);
				}
			}
			return cache;
		}

		void ApplyToHandNode(RE::Actor& a_actor, RE::BGSArtObject* a_artObject, const char* a_nodeName)
		{
			auto* node = a_actor.GetNodeByName(a_nodeName);
			if (!node) {
				logs::warn("Animation::TriggerHandGlow: nodo \"{}\" no encontrado en el esqueleto de \"{}\".",
					a_nodeName, a_actor.GetName());
				return;
			}

			a_actor.ApplyArtObject(a_artObject, Constants::kHandGlowDuration, nullptr, false, false, node);
		}
	}

	void TriggerHandGlow(RE::Actor& a_actor)
	{
		auto* artObject = GetHandGlowArtObject();
		if (!artObject) {
			return;
		}

		ApplyToHandNode(a_actor, artObject, Constants::kHandGlowLeftHandNodeName);
		ApplyToHandNode(a_actor, artObject, Constants::kHandGlowRightHandNodeName);
	}
}
