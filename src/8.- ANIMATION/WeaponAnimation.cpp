// Implementación de las animaciones del arma.
// Controla rotaciones, alineaciones y efectos visuales asociados.

#include "8.- ANIMATION/WeaponAnimation.h"

#include "1.- CORE/Constants.h"
#include "9.- MATH/RotationMath.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <numbers>
#include <thread>

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

	void TickShudder(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_baseRotation, float a_elapsedSeconds, float a_duration)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		// Chirp de fase continua: frecuencia f(t) sube en línea recta de
		// kStickShudderFrequencyStart a kStickShudderFrequencyEnd a lo
		// largo de a_duration (ya no siempre Constants::kStickShudderDuration,
		// ver TickShudder en el header); la fase es la integral de
		// 2π·f(t), en forma cerrada (no acumulada tick a tick, mismo
		// criterio que Throw::ComputeGravityDrop) para no arrastrar
		// deriva numérica ni depender del intervalo de tick.
		constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
		const float     freqSlope = (Constants::kStickShudderFrequencyEnd - Constants::kStickShudderFrequencyStart) / a_duration;
		const float     phase = twoPi * (Constants::kStickShudderFrequencyStart * a_elapsedSeconds + 0.5f * freqSlope * a_elapsedSeconds * a_elapsedSeconds);

		// Envolvente de amplitud: crece exponencialmente desde 0 hasta
		// kStickShudderMaxAngle, alcanzando kStickShudderAmplitudeRampFraction
		// de ese máximo justo al final de a_duration -- decayRate se
		// despeja de esa condición (forma cerrada, no ajustada a mano).
		const float decayRate = -std::log(1.0f - Constants::kStickShudderAmplitudeRampFraction) / a_duration;
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

	namespace
	{
		// Estado del zoom de apuntado: valor de worldFOV ANTES de restarle
		// el offset, para poder restaurarlo tal cual al desactivar.
		bool  g_aimZoomActive = false;
		float g_aimZoomSavedFOV = 0.0f;

		// Cancela la rampa en marcha (si la hay) antes de arrancar una nueva
		// -- sin esto, una activación seguida de una desactivación rápida
		// (o viceversa) dejaría dos hilos escribiendo el mismo campo de
		// cámara a la vez.
		std::shared_ptr<std::atomic<bool>> g_aimZoomRampActive;

		// Rampa manual propia (mismo patrón hilo-que-duerme-y-reencola de
		// StartTickLoop, ver 6.- PHYSICS/PhysicsManager.cpp): duración fija
		// (Constants::kAimZoomTransitionDuration), curva suave
		// (Math::SmoothStep01), llega exactamente a a_targetFOV y se detiene
		// sola.
		//
		// worldFOV (RE::PlayerCamera::RUNTIME_DATA2), no
		// ThirdPersonState::targetZoomOffset/currentZoomOffset (primer
		// intento, 2026-08-07, revertido): confirmado en el juego con una
		// prueba A/B (función completamente inerte vs. activa) que tocar
		// esos dos campos, aunque la lectura de vuelta mostraba el valor
		// exacto que escribíamos, causaba que la cámara en tercera persona
		// "volara" muchos metros por delante del personaje, detenida solo
		// por colisión real contra geometría (muros/vallas) -- ver
		// CHANGELOG.md. Esos campos están ligados al sistema de colisión/
		// posicionamiento de la cámara en tercera persona (ver
		// posOffsetExpected/posOffsetActual, mismo struct), un área que ya
		// no se toca. worldFOV es un parámetro de renderizado puro (ángulo
		// de visión), sin relación con la posición/colisión de la cámara --
		// mismo campo en primera y tercera persona, así que ya no hace
		// falta ninguna rama por perspectiva.
		void StartAimZoomRamp(float a_startFOV, float a_targetFOV)
		{
			auto active = std::make_shared<std::atomic<bool>>(true);
			g_aimZoomRampActive = active;

			// std::max evitado a propósito: Windows.h define max como macro
			// (mismo problema ya documentado en el proyecto, ver
			// Return::BeginReturn) -- un ternario en su lugar.
			const int rawStepCount = static_cast<int>(Constants::kAimZoomTransitionDuration / Constants::kTickDeltaSeconds);
			const int stepCount = rawStepCount > 1 ? rawStepCount : 1;

			std::thread([active, a_startFOV, a_targetFOV, stepCount]() {
				for (int step = 1; step <= stepCount && active->load(); ++step) {
					std::this_thread::sleep_for(Constants::kTickInterval);
					if (!active->load()) {
						return;
					}

					SKSE::GetTaskInterface()->AddTask([active, a_startFOV, a_targetFOV, step, stepCount]() {
						if (!active->load()) {
							return;
						}
						auto* camera = RE::PlayerCamera::GetSingleton();
						if (camera) {
							const float blend = Math::SmoothStep01(static_cast<float>(step) / static_cast<float>(stepCount));
							camera->GetRuntimeData2().worldFOV = a_startFOV + (a_targetFOV - a_startFOV) * blend;
						}
						if (step == stepCount) {
							active->store(false);
						}
					});
				}
			}).detach();
		}
	}

	void SetAimZoom(bool a_active)
	{
		if (a_active == g_aimZoomActive) {
			return;
		}

		auto* camera = RE::PlayerCamera::GetSingleton();
		if (!camera) {
			return;
		}

		if (!camera->IsInFirstPerson() && !camera->IsInThirdPerson()) {
			// Ni primera ni tercera persona (p. ej. cámara libre) -- nada
			// que zoomear, no se marca activo para no revertir un valor que
			// nunca se llegó a tocar.
			return;
		}

		if (g_aimZoomRampActive) {
			g_aimZoomRampActive->store(false);
			g_aimZoomRampActive.reset();
		}

		float startFOV;
		float targetFOV;

		if (a_active) {
			g_aimZoomSavedFOV = camera->GetRuntimeData2().worldFOV;
			startFOV = g_aimZoomSavedFOV;
			targetFOV = g_aimZoomSavedFOV + Constants::kAimZoomFOVOffset;
		} else {
			// Parte de donde esté ahora mismo (no necesariamente el valor
			// objetivo, si se soltó el botón antes de que la rampa de
			// entrada terminase) hacia el valor guardado antes de activar.
			startFOV = camera->GetRuntimeData2().worldFOV;
			targetFOV = g_aimZoomSavedFOV;
		}

		g_aimZoomActive = a_active;
		StartAimZoomRamp(startFOV, targetFOV);
	}
}
