// Destello con luz -- ver WeaponGlow.h.

#include "8.- ANIMATION/WeaponGlow.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Forms.h"
#include "1.- CORE/Scheduler.h"
#include "1.- CORE/Settings.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <cmath>
#include <numbers>

namespace Animation
{
	namespace
	{
		// Intentos de espera a que cargue el 3D (~800 ms).
		constexpr int kMax3DWaitAttempts = 50;

		// Único destello activo del plugin.
		RE::ObjectRefHandle g_activeHandle;
		Physics::TickToken  g_tickToken;

		// Luz dinámica creada con TESObjectLIGH::GenDynamic sobre el nodo raíz del destello.
		RE::NiPointer<RE::NiLight> g_niLight;

		// Fade pleno de la luz, leído del formulario (0 sin luz).
		float g_lightTargetFade = 0.0f;

		// Fase del fundido de encendido/apagado (escala de la malla y fade de la luz).
		enum class GlowPhase
		{
			kFadingIn,
			kSteady,
			kFadingOut
		};
		GlowPhase g_phase = GlowPhase::kFadingIn;
		float     g_phaseElapsed = 0.0f;

		// Avanza el fundido en curso. Lo llaman los bucles de tick del destello.
		void TickGlowFade(RE::TESObjectREFR& a_refr, float a_deltaSeconds)
		{
			if (g_phase == GlowPhase::kSteady) {
				return;
			}

			g_phaseElapsed += a_deltaSeconds;
			float t = g_phaseElapsed / Constants::kGlowFadeDurationSeconds;
			t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);

			if (g_phase == GlowPhase::kFadingOut) {
				t = 1.0f - t;
			} else if (t >= 1.0f) {
				g_phase = GlowPhase::kSteady;
			}

			if (auto* node3D = a_refr.Get3D()) {
				node3D->local.scale = t;
				node3D->world.scale = t;
			}
			if (g_niLight) {
				g_niLight->GetLightRuntimeData().fade = g_lightTargetFade * t;
			}
		}

		// Crea la luz del formulario y la engancha a a_root.
		void AttachGlowLight(RE::TESObjectREFR* a_ref, RE::NiAVObject* a_root)
		{
			auto* lightForm = Forms::weaponGlowLight;
			auto* rootNode = a_root ? a_root->AsNode() : nullptr;
			if (!lightForm || !rootNode || !a_ref) {
				return;
			}

			auto* niLight = lightForm->GenDynamic(a_ref, rootNode, 1, 1, 0);
			if (!niLight) {
				logs::warn("Animation::WeaponGlow: TESObjectLIGH::GenDynamic devolvió nullptr -- sin luz real.");
				return;
			}

			g_niLight = RE::NiPointer<RE::NiLight>(niLight);

			// Fade pleno tomado del formulario.
			g_lightTargetFade = lightForm->fade;
		}

		// Suelta la luz; se borra junto con la referencia del destello.
		void DetachGlowLight()
		{
			g_niLight.reset();
		}

		// Shader de la malla con scroll de "V Offset" (bajo el NiBillboardNode), escrito cada tick.
		RE::NiPointer<RE::BSEffectShaderProperty> g_shaderProperty;

		// Busca la geometría del primer NiBillboardNode hijo de a_root (por estructura, sin nombre).
		RE::BSGeometry* FindGlowScrollGeometry(RE::NiAVObject* a_root)
		{
			auto* rootNode = a_root ? a_root->AsNode() : nullptr;
			if (!rootNode) {
				return nullptr;
			}

			for (auto& child : rootNode->GetChildren()) {
				if (!child) {
					continue;
				}
				if (auto* billboard = netimmerse_cast<RE::NiBillboardNode*>(child.get())) {
					for (auto& grandchild : billboard->GetChildren()) {
						if (grandchild) {
							if (auto* geometry = grandchild->AsGeometry()) {
								return geometry;
							}
						}
					}
				}
			}

			return nullptr;
		}

		// Avanza texCoordOffset[0] cada tick (Constants::kGlowUVScrollSpeed).
		void TickGlowUVScroll(float a_deltaSeconds)
		{
			if (!g_shaderProperty) {
				return;
			}

			if (auto* material = g_shaderProperty->GetMaterial()) {
				material->texCoordOffset[0].y += Constants::kGlowUVScrollSpeed * a_deltaSeconds;
			}
		}

		// Shader de la malla "RingGlow" (Constants::kGlowRingGlowNodeName).
		RE::NiPointer<RE::BSEffectShaderProperty> g_ringGlowShaderProperty;
		float                                     g_pulseElapsed = 0.0f;

		// Nodo "RingGlow" y su rotación local inicial, base de su giro.
		RE::NiPointer<RE::NiAVObject> g_ringGlowNode;
		RE::NiMatrix3                 g_ringGlowBaseLocalRotation;
		float                         g_ringRotationElapsed = 0.0f;

		// Pulso de baseColorScale de "RingGlow" con una onda seno.
		void TickGlowPulse(float a_deltaSeconds)
		{
			if (!g_ringGlowShaderProperty) {
				return;
			}

			g_pulseElapsed += a_deltaSeconds;

			constexpr float twoPi = 2.0f * std::numbers::pi_v<float>;
			const float     sine01 = 0.5f * (1.0f + std::sin(twoPi * Constants::kGlowPulseFrequencyHz * g_pulseElapsed));
			const float     scale = Constants::kGlowPulseScaleMin + (Constants::kGlowPulseScaleMax - Constants::kGlowPulseScaleMin) * sine01;

			if (auto* material = g_ringGlowShaderProperty->GetMaterial()) {
				material->baseColorScale = scale;
			}
		}

		// Giro continuo de "RingGlow" sobre su eje.
		void TickGlowRingRotation(float a_deltaSeconds)
		{
			if (!g_ringGlowNode) {
				return;
			}

			g_ringRotationElapsed += a_deltaSeconds;

			RE::NiMatrix3 spin;
			spin.MakeRotation(Constants::kGlowRingRotationSpeed * g_ringRotationElapsed, Constants::kGlowRingRotationAxisLocal);
			g_ringGlowNode->local.rotate = g_ringGlowBaseLocalRotation * spin;
		}

		// Generación: descarta esperas de 3D pendientes si el destello se paró o se relevó.
		std::atomic<std::uint64_t> g_generation{ 0 };

		// Posición de la cabeza del martillo en la mano del jugador, reevaluada cada tick.
		RE::NiPoint3 GetPlayerHandGlowPosition()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			return GetGlowAnchorPosition(player ? ActorUtils::GetWeaponBone(*player) : nullptr);
		}

		// Pone el destello en kKeyframed y arranca su bucle siguiendo a_getTargetPosition.
		void StartTicking(RE::ObjectRefHandle a_handle, std::function<RE::NiPoint3()> a_getTargetPosition)
		{
			auto  ref = a_handle.get();
			auto* node3D = ref ? ref->Get3D() : nullptr;
			if (!node3D) {
				return;
			}

			node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);

			// Nace a escala 0; el fundido lo sube a 1.
			g_phase = GlowPhase::kFadingIn;
			g_phaseElapsed = 0.0f;
			node3D->local.scale = 0.0f;
			node3D->world.scale = 0.0f;

			// Shader del scroll, resuelto al cargar el 3D.
			if (auto* geometry = FindGlowScrollGeometry(node3D)) {
				g_shaderProperty = RE::NiPointer<RE::BSEffectShaderProperty>(
					skyrim_cast<RE::BSEffectShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get()));
				if (!g_shaderProperty) {
					logs::warn("Animation::WeaponGlow: geometría del destello sin BSEffectShaderProperty -- sin scroll de UV.");
				}
			} else {
				logs::warn("Animation::WeaponGlow: no se encontró la geometría del NiBillboardNode -- sin scroll de UV.");
			}

			// Shader y nodo de "RingGlow" para el pulso y el giro.
			g_pulseElapsed = 0.0f;
			g_ringRotationElapsed = 0.0f;
			if (auto* ringGlow = node3D->GetObjectByName(Constants::kGlowRingGlowNodeName)) {
				g_ringGlowNode = RE::NiPointer<RE::NiAVObject>(ringGlow);
				g_ringGlowBaseLocalRotation = ringGlow->local.rotate;
				if (auto* geometry = ringGlow->AsGeometry()) {
					g_ringGlowShaderProperty = RE::NiPointer<RE::BSEffectShaderProperty>(
						skyrim_cast<RE::BSEffectShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get()));
				}
			}
			if (!g_ringGlowNode) {
				logs::warn("Animation::WeaponGlow: no se encontró \"{}\" -- sin pulso de energía ni rotación.", Constants::kGlowRingGlowNodeName);
			} else if (!g_ringGlowShaderProperty) {
				logs::warn("Animation::WeaponGlow: \"{}\" sin BSEffectShaderProperty -- sin pulso de energía (la rotación sí aplica).", Constants::kGlowRingGlowNodeName);
			}

			// Luz enganchada al nodo raíz: se mueve con él.
			AttachGlowLight(ref.get(), node3D);

			g_tickToken = Physics::StartTickLoop(a_handle, [getPos = std::move(a_getTargetPosition)](RE::TESObjectREFR& a_refr, float a_deltaSeconds) {
				const auto pos = getPos();
				a_refr.SetPosition(pos);
				Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
				TickGlowUVScroll(a_deltaSeconds);
				TickGlowPulse(a_deltaSeconds);
				TickGlowRingRotation(a_deltaSeconds);
				TickGlowFade(a_refr, a_deltaSeconds);
				return true;
			});
		}

		// Espera a que cargue el 3D y arranca el bucle.
		void WaitFor3DThenStartTicking(RE::ObjectRefHandle a_handle, std::function<RE::NiPoint3()> a_getTargetPosition, int a_attemptsLeft, std::uint64_t a_generation)
		{
			if (g_generation.load() != a_generation) {
				return;
			}

			auto ref = a_handle.get();
			if (!ref) {
				return;
			}

			if (ref->Get3D()) {
				StartTicking(a_handle, std::move(a_getTargetPosition));
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::WeaponGlow: el 3D del destello nunca llegó a cargar, se aborta.");
				return;
			}

			(void)Scheduler::After(Constants::kTickInterval, [a_handle, getPos = std::move(a_getTargetPosition), a_attemptsLeft, a_generation]() mutable {
				WaitFor3DThenStartTicking(a_handle, std::move(getPos), a_attemptsLeft - 1, a_generation);
			});
		}
	}

	// Posición del nodo "Gold" (cabeza) bajo a_root + offset; sin él (el modelo del arma aún sin cargar,
	// p. ej. justo tras reequipar), la de a_root.
	RE::NiPoint3 GetGlowAnchorPosition(RE::NiAVObject* a_root)
	{
		if (!a_root) {
			return RE::NiPoint3{};
		}

		if (auto* goldNode = a_root->GetObjectByName(Constants::kWeaponHammerHeadNodeName)) {
			return goldNode->world.translate + goldNode->world.rotate * Constants::kGlowAnchorLocalOffset;
		}

		return a_root->world.translate;
	}

	bool StartWeaponGlow(RE::Actor& a_actor, bool a_checkSetting)
	{
		// Desactivable con [VFX] WeaponLight, salvo a_checkSetting=false (power attacks).
		if (a_checkSetting && !Settings::GetWeaponLight()) {
			return false;
		}

		if (g_activeHandle) {
			if (g_phase != GlowPhase::kFadingOut) {
				return false;
			}

			// El anterior aún se estaba apagando: se cierra ya y se invalida su cierre diferido.
			++g_generation;
			Physics::CancelTickLoop(g_tickToken);
			g_shaderProperty.reset();
			g_ringGlowShaderProperty.reset();
			g_ringGlowNode.reset();
			DetachGlowLight();
			Physics::DestroyReference(g_activeHandle);
			g_activeHandle = {};
		}

		auto* form = Forms::weaponGlowActivator;
		if (!form) {
			return false;
		}

		auto ref = a_actor.PlaceObjectAtMe(form, false);
		if (!ref) {
			logs::warn("Animation::StartWeaponGlow: PlaceObjectAtMe devolvió nullptr.");
			return false;
		}

		// Sin activación: el jugador no puede recogerlo.
		ref->SetActivationBlocked(true);

		g_activeHandle = RE::ObjectRefHandle(ref.get());

		const auto generation = ++g_generation;
		WaitFor3DThenStartTicking(g_activeHandle, GetPlayerHandGlowPosition, kMax3DWaitAttempts, generation);
		return true;
	}

	void RetargetWeaponGlowToReplica(RE::ObjectRefHandle a_handle)
	{
		if (!g_activeHandle) {
			return;
		}

		auto  replica = a_handle.get();
		auto* root = replica ? replica->Get3D() : nullptr;
		if (!replica || !root) {
			logs::warn("Animation::RetargetWeaponGlowToReplica: réplica sin 3D todavía.");
			return;
		}

		// Cancela el bucle anterior antes de arrancar el nuevo.
		Physics::CancelTickLoop(g_tickToken);

		// Si la réplica desaparece, se queda en su última posición.
		g_tickToken = Physics::StartTickLoop(g_activeHandle, [handle = a_handle, lastPosition = GetGlowAnchorPosition(root)](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			auto  replicaRef = handle.get();
			auto* replicaRoot = replicaRef ? replicaRef->Get3D() : nullptr;
			if (replicaRoot) {
				lastPosition = GetGlowAnchorPosition(replicaRoot);
			}
			a_refr.SetPosition(lastPosition);
			Physics::SyncHavok(a_refr, lastPosition, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
			TickGlowUVScroll(a_deltaSeconds);
			TickGlowPulse(a_deltaSeconds);
			TickGlowRingRotation(a_deltaSeconds);
			TickGlowFade(a_refr, a_deltaSeconds);
			return true;
		});
	}

	void RetargetWeaponGlowToActor(RE::Actor& a_actor)
	{
		if (!g_activeHandle) {
			return;
		}

		if (!ActorUtils::GetWeaponBone(a_actor)) {
			logs::warn("Animation::RetargetWeaponGlowToActor: hueso \"{}\" no encontrado.", Constants::kWeaponNodeName);
			return;
		}

		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = Physics::StartTickLoop(g_activeHandle, [](RE::TESObjectREFR& a_refr, float a_deltaSeconds) {
			const auto pos = GetPlayerHandGlowPosition();
			a_refr.SetPosition(pos);
			Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
			TickGlowUVScroll(a_deltaSeconds);
			TickGlowPulse(a_deltaSeconds);
			TickGlowRingRotation(a_deltaSeconds);
			TickGlowFade(a_refr, a_deltaSeconds);
			return true;
		});
	}

	void StopWeaponGlow()
	{
		if (!g_activeHandle || g_phase == GlowPhase::kFadingOut) {
			// Sin destello o ya apagándose.
			return;
		}

		// Fundido de salida y borrado pasado Constants::kGlowFadeDuration (si nadie arrancó otro).
		g_phase = GlowPhase::kFadingOut;
		g_phaseElapsed = 0.0f;

		const auto generation = ++g_generation;
		(void)Scheduler::After(Constants::kGlowFadeDuration, [generation]() {
			if (g_generation.load() != generation) {
				return;
			}

			Physics::CancelTickLoop(g_tickToken);

			g_shaderProperty.reset();
			g_ringGlowShaderProperty.reset();
			g_ringGlowNode.reset();
			DetachGlowLight();

			if (g_activeHandle) {
				Physics::DestroyReference(g_activeHandle);
				g_activeHandle = {};
			}
		});
	}
}
