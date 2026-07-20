// Implementación de las animaciones del arma.
// Controla rotaciones, alineaciones y efectos visuales asociados.

#include "8.- ANIMATION/WeaponAnimation.h"

#include "1.- CORE/Constants.h"

namespace Animation
{
	void TickSpin(RE::TESObjectREFR& a_refr, float a_elapsedSeconds)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		spinNode->local.rotate.MakeRotation(Constants::kSpinAngularSpeed * a_elapsedSeconds, Constants::kSpinAxisLocal);
	}

	RE::NiTransform GetVisualTransform(RE::NiAVObject& a_root)
	{
		if (auto* spinNode = a_root.GetObjectByName(Constants::kWeaponSpinNodeName)) {
			return spinNode->world;
		}

		return a_root.world;
	}
}
