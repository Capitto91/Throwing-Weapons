// Destello de manos -- ver HandGlow.h.

#include "8.- ANIMATION/HandGlow.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Forms.h"
#include "1.- CORE/Settings.h"

namespace Animation
{
	namespace
	{
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
		// Desactivable desde [VFX] HandEffect (Settings).
		if (!Settings::GetHandEffect()) {
			return;
		}

		auto* artObject = Forms::handGlowArtObject;
		if (!artObject) {
			return;
		}

		ApplyToHandNode(a_actor, artObject, Constants::kHandGlowLeftHandNodeName);
		ApplyToHandNode(a_actor, artObject, Constants::kHandGlowRightHandNodeName);
	}
}
