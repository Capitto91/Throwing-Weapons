// Implementación de las animaciones del arma.
// Controla rotaciones, alineaciones y efectos visuales asociados.

#include "8.- ANIMATION/WeaponAnimation.h"

#include "1.- CORE/Constants.h"

#include <cmath>
#include <numbers>

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

	RE::NiTransform GetVisualTransform(RE::NiAVObject& a_root)
	{
		if (auto* spinNode = a_root.GetObjectByName(Constants::kWeaponSpinNodeName)) {
			return spinNode->world;
		}

		return a_root.world;
	}
}
