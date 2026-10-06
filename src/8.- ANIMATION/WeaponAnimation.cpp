// Animación de la réplica -- ver WeaponAnimation.h.

#include "8.- ANIMATION/WeaponAnimation.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Forms.h"
#include "11.- SKYRIM/ActorUtils.h"
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
		// El hueso del arma no es la malla: la malla es su hijo.
		auto* weaponNode = ActorUtils::GetWeaponBone(a_actor);
		auto* asNode = weaponNode ? netimmerse_cast<RE::NiNode*>(weaponNode) : nullptr;
		if (!asNode || asNode->GetChildren().empty()) {
			logs::warn("Animation::GetEquippedWeaponWorldRotation: nodo \"{}\" no encontrado o sin hijos.", Constants::kWeaponNodeName);
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
		auto* handNode = ActorUtils::GetWeaponBone(a_actor);
		if (!handNode) {
			logs::warn("Animation::GetHandBoneWorldRotation: hueso \"{}\" no encontrado.", Constants::kWeaponNodeName);
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

	bool SetTrigger(Gesture a_gesture, bool a_active)
	{
		RE::TESGlobal* global = nullptr;
		switch (a_gesture) {
		case Gesture::kThrow:
			global = Forms::throwTriggerGlobal;
			break;
		case Gesture::kCall:
			global = Forms::callTriggerGlobal;
			break;
		case Gesture::kCatch:
			global = Forms::catchTriggerGlobal;
			break;
		case Gesture::kSlam:
			global = Forms::slamTriggerGlobal;
			break;
		}

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
			auto* weaponNode = root ? root->GetObjectByName(Constants::kWeaponNodeName) : nullptr;
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
			logs::warn("Animation::SetEquippedWeaponHidden: nodo \"{}\" no encontrado o sin hijos.", Constants::kWeaponNodeName);
		}
		return applied;
	}

	std::int32_t GetRightHandTypeFor(const RE::TESBoundObject* a_weapon)
	{
		const auto* weapon = a_weapon ? a_weapon->As<RE::TESObjectWEAP>() : nullptr;
		if (!weapon) {
			return Constants::kRightHandTypeOneHanded;
		}

		switch (const auto type = weapon->GetWeaponType()) {
		case RE::WEAPON_TYPE::kOneHandSword:
		case RE::WEAPON_TYPE::kOneHandDagger:
		case RE::WEAPON_TYPE::kOneHandAxe:
		case RE::WEAPON_TYPE::kOneHandMace:
			return static_cast<std::int32_t>(type);
		default:
			return Constants::kRightHandTypeOneHanded;
		}
	}
}
