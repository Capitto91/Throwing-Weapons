// Implementación de las animaciones del arma.
// Controla rotaciones, alineaciones y efectos visuales asociados.

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

		// Composición permanente, no un fundido temporal: a_baseLocal (la
		// pose real de la que partía el arma al empezar este tramo) sigue
		// pintando durante todo el vuelo, no solo durante la rampa de
		// arranque del giro -- ver el comentario del header (bug "se
		// aplana momentos después", 2026-08-06).
		spinNode->local.rotate = a_baseLocal * ComputeSpinRotation(a_elapsedSeconds);
	}

	void TickSpinStraighten(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_blendFromLocal, const RE::NiMatrix3& a_targetLocal, float a_blend)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		// Curva suave (smoothstep) en vez de una interpolación lineal
		// brusca -- el enderezado debe notarse como un frenado gradual del
		// giro, no un tirón.
		const float smoothBlend = Math::SmoothStep01(a_blend);
		spinNode->local.rotate = Math::SlerpRotation(a_blendFromLocal, a_targetLocal, smoothBlend);
	}

	RE::NiMatrix3 GetSpinLocalRotation(RE::TESObjectREFR& a_refr)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		return spinNode ? spinNode->local.rotate : RE::NiMatrix3{};
	}

	RE::NiMatrix3 ComputeImpactAlignment(const RE::NiMatrix3& a_rootWorld, const RE::NiMatrix3& a_currentLocal, const RE::NiPoint3& a_travelDirection)
	{
		// Dirección mundial hacia la que apunta Constants::kImpactAxisLocal
		// AHORA MISMO (con la rotación local real que lleve el nodo de
		// giro en este instante, no una referencia de "reposo" asumida --
		// ver el comentario del header) -- punto de partida para calcular
		// cuánto hay que girar para que esa dirección coincida con
		// a_travelDirection.
		const RE::NiMatrix3 currentWorld = a_rootWorld * a_currentLocal;
		const RE::NiPoint3  currentHeadWorldDir = currentWorld * Constants::kImpactAxisLocal;
		const RE::NiMatrix3 align = Math::ShortestArcRotation(currentHeadWorldDir, a_travelDirection);
		const RE::NiMatrix3 targetWorld = align * currentWorld;
		return Math::LocalRotationFromWorld(a_rootWorld, targetWorld);
	}

	RE::NiMatrix3 GetEquippedWeaponWorldRotation(RE::Actor& a_actor)
	{
		// "WEAPON" es el hueso de enganche, no la malla -- mismo criterio
		// que SetEquippedWeaponHidden (ver ese comentario para el porqué
		// de bajar a los hijos en vez de usar el hueso directamente).
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

	namespace
	{
		// Sustituye a la graph variable + BehaviorDataInjector (ver
		// CLAUDE.md, 2026-08-05): un TESGlobal real de la Creation Kit no
		// tiene el problema de fondo que sí tienen las graph variables de
		// Havok (solo hay almacenamiento real para nombres ya declarados en
		// la tabla interna del behavior) -- BDI existía solo para inyectar
		// esa entrada en tiempo de ejecución; un Global no la necesita en
		// absoluto, es almacenamiento nativo del motor. La condición
		// CompareValues del submod de OAR compara contra este mismo Global
		// (Value A -> "form", no "graphVariable" -- ver el config.json real
		// de cada submod). Búsqueda perezosa (primera llamada, no en
		// carga del plugin) cacheada en la propia variable estática -- mismo
		// motivo que el resto de búsquedas por EditorID del proyecto
		// (p. ej. Combat::BeginEmbeddedEffect): las formas no están
		// garantizadas disponibles antes de kDataLoaded.
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

	void SetAnimationDriven(RE::Actor& a_actor, bool a_active)
	{
		a_actor.SetGraphVariableBool(Constants::kAnimationDrivenGraphVariable, a_active);
	}

	bool SetEquippedWeaponHidden(RE::Actor& a_actor, bool a_hidden)
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
			return false;
		}

		for (auto& child : asNode->GetChildren()) {
			if (child) {
				child->GetFlags().set(a_hidden, RE::NiAVObject::Flag::kHidden);
			}
		}

		return true;
	}
}
