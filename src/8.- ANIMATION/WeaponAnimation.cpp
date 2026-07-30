// Implementación de las animaciones del arma.
// Controla rotaciones, alineaciones y efectos visuales asociados.

#include "8.- ANIMATION/WeaponAnimation.h"

#include "1.- CORE/Constants.h"

#include <cmath>
#include <numbers>

namespace Animation
{
	namespace
	{
		// Rampa de arranque del giro (a petición del usuario, velocidad
		// angular ya no constante desde el instante cero): dos tramos
		// empalmados en forma cerrada, mismo criterio que
		// Throw::ComputeGravityDrop pero un orden de derivada más abajo
		// (ahí se rampeaba la aceleración lineal hasta un máximo, aquí se
		// rampea la velocidad angular hasta Constants::kSpinAngularSpeed).
		// Continuo en ángulo y en velocidad angular en el empalme
		// (t = kSpinRampDuration): verificado por integración directa,
		// ambas ramas coinciden ahí sin salto.
		float ComputeSpinAngle(float a_elapsedSeconds)
		{
			constexpr float rampDuration = Constants::kSpinRampDuration;
			if constexpr (rampDuration <= 0.0f) {
				return Constants::kSpinAngularSpeed * a_elapsedSeconds;
			}

			if (a_elapsedSeconds < rampDuration) {
				// ω(t) = (ωmax/rampDuration)·t (aceleración angular
				// constante) -> ángulo(t) = ½·(ωmax/rampDuration)·t².
				return Constants::kSpinAngularSpeed * a_elapsedSeconds * a_elapsedSeconds / (2.0f * rampDuration);
			}

			const float angleAtRampEnd = Constants::kSpinAngularSpeed * rampDuration / 2.0f;
			return angleAtRampEnd + Constants::kSpinAngularSpeed * (a_elapsedSeconds - rampDuration);
		}
	}

	void TickSpin(RE::TESObjectREFR& a_refr, float a_elapsedSeconds)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		spinNode->local.rotate.MakeRotation(ComputeSpinAngle(a_elapsedSeconds), Constants::kSpinAxisLocal);
	}

	void TickShudder(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_baseRotation, float a_elapsedSeconds)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		// Chirp de fase continua: frecuencia f(t) sube en línea recta de
		// kStickShudderFrequencyStart a kStickShudderFrequencyEnd a lo
		// largo de kStickShudderDuration; la fase es la integral de
		// 2π·f(t), en forma cerrada (no acumulada tick a tick, mismo
		// criterio que Throw::ComputeGravityDrop) para no arrastrar
		// deriva numérica ni depender del intervalo de tick.
		constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
		constexpr float freqSlope = (Constants::kStickShudderFrequencyEnd - Constants::kStickShudderFrequencyStart) / Constants::kStickShudderDuration;
		const float     phase = twoPi * (Constants::kStickShudderFrequencyStart * a_elapsedSeconds + 0.5f * freqSlope * a_elapsedSeconds * a_elapsedSeconds);

		// Envolvente de amplitud: crece exponencialmente desde 0 hasta
		// kStickShudderMaxAngle, alcanzando kStickShudderAmplitudeRampFraction
		// de ese máximo justo al final de kStickShudderDuration -- decayRate
		// se despeja de esa condición (forma cerrada, no ajustada a mano).
		const float decayRate = -std::log(1.0f - Constants::kStickShudderAmplitudeRampFraction) / Constants::kStickShudderDuration;
		const float amplitude = Constants::kStickShudderMaxAngle * (1.0f - std::exp(-decayRate * a_elapsedSeconds));

		const float angle = amplitude * std::sin(phase);

		// Compuesta sobre la rotación base (con la que se quedó clavada),
		// no sustituida: a diferencia de TickSpin (rotación absoluta desde
		// el reposo), aquí el arma llega de un ángulo de vuelo arbitrario,
		// así que escribir una rotación absoluta desde cero en a_elapsedSeconds=0
		// producía un salto visual perceptible (reportado por el usuario
		// como "cambia de posición" al empezar a temblar).
		RE::NiMatrix3 wobble;
		wobble.MakeRotation(angle, Constants::kStickShudderAxisLocal);
		spinNode->local.rotate = a_baseRotation * wobble;
	}

	void SetThrowTrigger(RE::Actor& a_actor, bool a_active)
	{
		// Float, no Bool -- la condición CompareValues montada en el editor
		// de OAR (ver _reference/PLAN-OAR.md) compara esta graph variable
		// como Float contra 1.0f, para no arriesgar un desajuste de tipo.
		a_actor.SetGraphVariableFloat(Constants::kThrowTriggerGraphVariable, a_active ? 1.0f : 0.0f);
	}

	void SetAnimationDriven(RE::Actor& a_actor, bool a_active)
	{
		a_actor.SetGraphVariableBool(Constants::kAnimationDrivenGraphVariable, a_active);
	}

	void SetEquippedWeaponHidden(RE::Actor& a_actor, bool a_hidden)
	{
		// "WEAPON" es el hueso de enganche del esqueleto, no la malla del
		// arma en sí -- confirmado con log de diagnóstico (ver
		// _reference/PLAN-OAR.md): tiene un único hijo, un BSFadeNode, que
		// es la malla real. Poner kHidden en el propio "WEAPON" no ocultaba
		// nada visualmente (comprobado en el juego, dos intentos: primero
		// SetAppCulled, luego kHidden en este mismo nodo) -- el pipeline de
		// renderizado del arma equipada no respeta kHidden heredado del
		// padre de esta forma. Hay que ponerlo en el propio BSFadeNode.
		auto* weaponNode = a_actor.GetNodeByName("WEAPON");
		auto* asNode = weaponNode ? netimmerse_cast<RE::NiNode*>(weaponNode) : nullptr;
		if (!asNode || asNode->GetChildren().empty()) {
			logs::warn("Animation::SetEquippedWeaponHidden: nodo \"WEAPON\" no encontrado o sin hijos.");
			return;
		}

		for (auto& child : asNode->GetChildren()) {
			if (child) {
				child->GetFlags().set(a_hidden, RE::NiAVObject::Flag::kHidden);
			}
		}
	}
}
