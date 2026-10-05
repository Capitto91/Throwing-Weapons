// Animación de la réplica -- ver WeaponAnimation.h.

#include "8.- ANIMATION/WeaponAnimation.h"

#include "1.- CORE/Constants.h"
#include "9.- MATH/RotationMath.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Animation
{
	namespace
	{
		// Ángulo con rampa: aceleración angular constante hasta kSpinRampDuration, luego velocidad fija.
		float ComputeSpinAngle(float a_elapsedSeconds)
		{
			constexpr float rampDuration = Constants::kSpinRampDuration;
			if constexpr (rampDuration <= 0.0f) {
				return Constants::kSpinAngularSpeed * a_elapsedSeconds;
			}

			if (a_elapsedSeconds < rampDuration) {
				// ángulo(t) = ½·(ωmax/rampDuration)·t².
				return Constants::kSpinAngularSpeed * a_elapsedSeconds * a_elapsedSeconds / (2.0f * rampDuration);
			}

			const float angleAtRampEnd = Constants::kSpinAngularSpeed * rampDuration / 2.0f;
			return angleAtRampEnd + Constants::kSpinAngularSpeed * (a_elapsedSeconds - rampDuration);
		}

		RE::NiMatrix3 ComputeSpinRotation(float a_elapsedSeconds)
		{
			RE::NiMatrix3 rotation;
			rotation.MakeRotation(ComputeSpinAngle(a_elapsedSeconds), Constants::kSpinAxisLocal);
			return rotation;
		}
	}

	void TickSpin(RE::TESObjectREFR& a_refr, float a_elapsedSeconds, const RE::NiMatrix3& a_baseLocal)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		// La pose base se mantiene durante todo el vuelo.
		spinNode->local.rotate = a_baseLocal * ComputeSpinRotation(a_elapsedSeconds);
	}

	void TickSpinStraighten(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_blendFromLocal, const RE::NiMatrix3& a_targetLocal, float a_blend)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		// Curva suave para que el enderezado frene el giro sin tirón.
		const float smoothBlend = Math::SmoothStep01(a_blend);
		spinNode->local.rotate = Math::SlerpRotation(a_blendFromLocal, a_targetLocal, smoothBlend);
	}

	RE::NiMatrix3 GetSpinLocalRotation(RE::TESObjectREFR& a_refr)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		return spinNode ? spinNode->local.rotate : RE::NiMatrix3{};
	}

	RE::NiMatrix3 GetEquippedWeaponWorldRotation(RE::Actor& a_actor)
	{
		// "WEAPON" es el hueso; la malla es su hijo.
		auto* weaponNode = a_actor.GetNodeByName("WEAPON");
		auto* asNode = weaponNode ? netimmerse_cast<RE::NiNode*>(weaponNode) : nullptr;
		if (!asNode || asNode->GetChildren().empty()) {
			logs::warn("Animation::GetEquippedWeaponWorldRotation: nodo \"WEAPON\" no encontrado o sin hijos.");
			return RE::NiMatrix3{};
		}

		for (auto& child : asNode->GetChildren()) {
			if (child) {
				return child->world.rotate;
			}
		}

		return RE::NiMatrix3{};
	}

	RE::NiMatrix3 GetHandBoneWorldRotation(RE::Actor& a_actor)
	{
		auto* handNode = a_actor.GetNodeByName("WEAPON");
		if (!handNode) {
			logs::warn("Animation::GetHandBoneWorldRotation: hueso \"WEAPON\" no encontrado.");
			return RE::NiMatrix3{};
		}

		return handNode->world.rotate;
	}

	void TickShudder(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_baseRotation, float a_elapsedSeconds, float a_duration)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		// Chirp de fase continua de kStickShudderFrequencyStart a End en forma cerrada.
		constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
		const float     freqSlope = (Constants::kStickShudderFrequencyEnd - Constants::kStickShudderFrequencyStart) / a_duration;
		const float     phase = twoPi * (Constants::kStickShudderFrequencyStart * a_elapsedSeconds + 0.5f * freqSlope * a_elapsedSeconds * a_elapsedSeconds);

		// Amplitud con crecimiento exponencial hasta kStickShudderMaxAngle.
		const float decayRate = -std::log(1.0f - Constants::kStickShudderAmplitudeRampFraction) / a_duration;
		const float amplitude = Constants::kStickShudderMaxAngle * (1.0f - std::exp(-decayRate * a_elapsedSeconds));

		const float angle = amplitude * std::sin(phase);

		// Oscilación compuesta sobre la rotación con la que se clavó, sin salto.
		RE::NiMatrix3 wobble;
		wobble.MakeRotation(angle, Constants::kStickShudderAxisLocal);
		spinNode->local.rotate = a_baseRotation * wobble;
	}

	namespace
	{
		// Global buscado por EditorID la primera vez y cacheado.
		RE::TESGlobal* LookupTriggerGlobal(const char* a_editorID)
		{
			auto* global = RE::TESForm::LookupByEditorID<RE::TESGlobal>(a_editorID);
			if (!global) {
				logs::warn("Animation: no se encontró el Global '{}' (revisa que exista en la Creation Kit/xEdit, con ese EditorID exacto).", a_editorID);
			}
			return global;
		}
	}

	void SetThrowTrigger(RE::Actor&, bool a_active)
	{
		static RE::TESGlobal* global = LookupTriggerGlobal(Constants::kThrowTriggerGlobalEditorID);
		if (global) {
			global->value = a_active ? 1.0f : 0.0f;
		}
	}

	void SetCallTrigger(RE::Actor&, bool a_active)
	{
		static RE::TESGlobal* global = LookupTriggerGlobal(Constants::kCallTriggerGlobalEditorID);
		if (global) {
			global->value = a_active ? 1.0f : 0.0f;
		}
	}

	void SetCatchTrigger(RE::Actor&, bool a_active)
	{
		static RE::TESGlobal* global = LookupTriggerGlobal(Constants::kCatchTriggerGlobalEditorID);
		if (global) {
			global->value = a_active ? 1.0f : 0.0f;
		}
	}

	bool SetSlamTrigger(RE::Actor&, bool a_active)
	{
		static RE::TESGlobal* global = LookupTriggerGlobal(Constants::kSlamTriggerGlobalEditorID);
		if (!global) {
			return false;
		}
		global->value = a_active ? 1.0f : 0.0f;
		return true;
	}

	void SetAnimationDriven(RE::Actor& a_actor, bool a_active)
	{
		a_actor.SetGraphVariableBool(Constants::kAnimationDrivenGraphVariable, a_active);
	}

	bool SetEquippedWeaponHidden(RE::Actor& a_actor, bool a_hidden)
	{
		// kHidden en el BSFadeNode hijo de "WEAPON" (en el hueso no oculta nada), en los dos esqueletos:
		// el arma cuelga de ambos y solo se ve el de la cámara activa.
		bool applied = false;
		for (const bool firstPerson : { false, true }) {
			auto* root = a_actor.Get3D(firstPerson);
			auto* weaponNode = root ? root->GetObjectByName("WEAPON") : nullptr;
			auto* asNode = weaponNode ? netimmerse_cast<RE::NiNode*>(weaponNode) : nullptr;
			if (!asNode) {
				continue;
			}

			for (auto& child : asNode->GetChildren()) {
				if (child) {
					child->GetFlags().set(a_hidden, RE::NiAVObject::Flag::kHidden);
					applied = true;
				}
			}
		}

		if (!applied) {
			logs::warn("Animation::SetEquippedWeaponHidden: nodo \"WEAPON\" no encontrado o sin hijos.");
		}
		return applied;
	}

	namespace
	{
		// Hijos ocultos de los huesos "WEAPON" de los dos esqueletos, con su hueso; vacío si la mano derecha tiene algo.
		std::vector<std::pair<RE::NiNode*, RE::NiPointer<RE::NiAVObject>>> FindOrphanWeaponModels(RE::Actor& a_actor)
		{
			std::vector<std::pair<RE::NiNode*, RE::NiPointer<RE::NiAVObject>>> orphans;
			if (a_actor.GetEquippedObject(false)) {
				return orphans;
			}

			for (const bool firstPerson : { false, true }) {
				auto* root = a_actor.Get3D(firstPerson);
				auto* weaponNode = root ? root->GetObjectByName("WEAPON") : nullptr;
				auto* asNode = weaponNode ? netimmerse_cast<RE::NiNode*>(weaponNode) : nullptr;
				if (!asNode) {
					continue;
				}

				for (auto& child : asNode->GetChildren()) {
					if (child && child->GetFlags().any(RE::NiAVObject::Flag::kHidden)) {
						orphans.emplace_back(asNode, child);
					}
				}
			}
			return orphans;
		}
	}

	bool HasOrphanWeaponModel(RE::Actor& a_actor)
	{
		return !FindOrphanWeaponModels(a_actor).empty();
	}

	void DetachOrphanWeaponModels(RE::Actor& a_actor)
	{
		for (auto& [weaponNode, child] : FindOrphanWeaponModels(a_actor)) {
			logs::warn("Animation::DetachOrphanWeaponModels: el motor no retiró '{}' tras el desequipado; se desengancha a mano.", child->name.c_str());
			weaponNode->DetachChild(child.get());
		}
	}
}
