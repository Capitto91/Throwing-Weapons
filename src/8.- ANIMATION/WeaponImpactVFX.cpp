// Implementación del VFX de impacto. Ver el header para la arquitectura
// completa (fire-and-forget, sin estado global) y el porqué frente a
// WeaponVFX.cpp/WeaponGlow.cpp.

#include "8.- ANIMATION/WeaponImpactVFX.h"

#include "1.- CORE/Constants.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <algorithm>
#include <thread>

namespace Animation
{
	namespace
	{
		// Mismo margen que Physics::SpawnReplica/Animation::WeaponVFX/
		// Animation::WeaponGlow -- ~800ms de sobra para lo que suele tardar
		// el 3D de una referencia recién colocada en cargar en segundo
		// plano.
		constexpr int kMax3DWaitAttempts = 50;

		// Formulario resuelto una sola vez por sesión -- mismo patrón que
		// GetOnActivatorForm (WeaponVFX.cpp)/GetGlowActivatorForm
		// (WeaponGlow.cpp).
		RE::TESBoundObject* GetImpactActivatorForm()
		{
			static RE::TESBoundObject* cache = nullptr;
			static bool                lookupDone = false;
			if (!lookupDone) {
				lookupDone = true;
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					cache = dataHandler->LookupForm<RE::TESObjectACTI>(Constants::kImpactVfxActivatorLocalFormID, Constants::kSoundPluginName);
				}
				if (!cache) {
					logs::warn("Animation::WeaponImpactVFX: no se encontró el Activator (FormID local 0x{:03X}) en \"{}\".",
						Constants::kImpactVfxActivatorLocalFormID, Constants::kSoundPluginName);
				}
			}
			return cache;
		}

		// Curva cerrada del pulso "crece y luego mengua" sobre el nodo
		// "glow" -- no acumulada tick a tick, mismo estilo que
		// Throw::ComputeGravityDrop: recibe el tiempo total transcurrido,
		// no un delta, y calcula el valor exacto para ese instante. Perfil
		// de dos tramos (subida rápida a pico, bajada más lenta),
		// decodificado del NIF de referencia vanilla
		// (fxshockcloakhandeffects.nif, secuencias mCast/mCastCon, nodo
		// "glow") -- ver Constants::kImpactPulse* para el detalle y el
		// porqué de cada valor.
		//
		// Aplica el mismo factor de escala tanto al tamaño del nodo
		// (NiAVObject::local.scale/world.scale, mismo campo que
		// Animation::WeaponGlow::TickGlowFade) como al brillo del shader
		// (BSEffectShaderMaterial::baseColorScale, mismo campo que
		// Animation::WeaponGlow::TickGlowPulse) -- decisión de v1 por
		// simplicidad, ver Constants.h.
		void ApplyImpactPulse(RE::NiAVObject* a_glowNode, RE::BSEffectShaderProperty* a_shaderProperty, float a_elapsed)
		{
			const float tNorm = std::clamp(a_elapsed / Constants::kImpactPulseDurationSeconds, 0.0f, 1.0f);

			float scale;
			if (tNorm <= Constants::kImpactPulseGrowFraction) {
				const float u = Constants::kImpactPulseGrowFraction > 0.0f ? tNorm / Constants::kImpactPulseGrowFraction : 1.0f;
				const float smooth = u * u * (3.0f - 2.0f * u);
				scale = Constants::kImpactPulseScaleBase + (Constants::kImpactPulseScalePeak - Constants::kImpactPulseScaleBase) * smooth;
			} else {
				const float shrinkSpan = 1.0f - Constants::kImpactPulseGrowFraction;
				const float u = shrinkSpan > 0.0f ? (tNorm - Constants::kImpactPulseGrowFraction) / shrinkSpan : 1.0f;
				const float smooth = u * u * (3.0f - 2.0f * u);
				scale = Constants::kImpactPulseScalePeak + (Constants::kImpactPulseScaleEnd - Constants::kImpactPulseScalePeak) * smooth;
			}

			if (a_glowNode) {
				a_glowNode->local.scale = scale;
				a_glowNode->world.scale = scale;
			}
			if (a_shaderProperty) {
				if (auto* material = a_shaderProperty->GetMaterial()) {
					material->baseColorScale = scale;
				}
			}
		}

		// Arranca el único bucle de tick de esta instancia: aplica el pulso
		// sobre el nodo "glow" ya resuelto, autoterminándose solo al
		// superar Constants::kImpactPulseDurationSeconds -- nadie necesita
		// cancelarlo desde fuera (fire-and-forget, ver el header). Sin
		// partículas en v1 (ver Constants.h/NIF-PARAMETERS.md) -- decisión
		// del usuario, 2026-08-28: el birth rate no se puede animar por
		// código (confirmado repetidas veces en este proyecto) y hornear
		// la NiControllerSequence en NifSkope resultó más complicado de lo
		// que compensaba para esta primera versión. El .nif puede seguir
		// llevando los dos NiParticleSystem sin usar (inertes, sin ningún
		// NiControllerManager que los active) o el usuario puede quitarlos
		// del todo -- indistinto para este código, que nunca los toca.
		void StartTicking(RE::ObjectRefHandle a_handle, RE::NiAVObject* a_glowNode, RE::NiPointer<RE::BSEffectShaderProperty> a_shaderProperty)
		{
			// Token descartado a propósito -- ver el header, esta instancia
			// nunca necesita cancelar su propio bucle desde fuera (se
			// autotermina solo, y Physics::StartTickLoop no exige que el
			// llamante lo retenga para que el hilo siga vivo).
			[[maybe_unused]] auto token = Physics::StartTickLoop(a_handle, [a_glowNode, shaderProperty = std::move(a_shaderProperty), elapsed = 0.0f](RE::TESObjectREFR&, float a_deltaSeconds) mutable {
				elapsed += a_deltaSeconds;
				ApplyImpactPulse(a_glowNode, shaderProperty.get(), elapsed);
				return elapsed < Constants::kImpactPulseDurationSeconds;
			});
		}

		// Sondeo con el mismo patrón hilo-que-duerme-y-reencola del resto
		// del proyecto -- ver Animation::WeaponVFX::WaitFor3DThenStartTicking.
		// Sin contador de generación (a diferencia de esa función): no hay
		// ningún Stop() externo que pueda invalidar esta espera a mitad de
		// camino, cada instancia es independiente (ver el header).
		void WaitFor3DThenStart(RE::ObjectRefHandle a_handle, RE::NiPoint3 a_position, int a_attemptsLeft)
		{
			auto ref = a_handle.get();
			if (!ref) {
				return;
			}

			if (auto* node3D = ref->Get3D()) {
				node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);
				ref->SetPosition(a_position);
				Physics::SyncHavok(*ref, a_position, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });

				auto* glowNode = node3D->GetObjectByName(Constants::kImpactGlowNodeName);
				if (!glowNode) {
					logs::warn("Animation::WeaponImpactVFX: nodo \"{}\" no encontrado -- sin pulso de escala.", Constants::kImpactGlowNodeName);
				}

				RE::NiPointer<RE::BSEffectShaderProperty> shaderProperty;
				if (auto* glowGeometryNode = node3D->GetObjectByName(Constants::kImpactGlowGeometryNodeName)) {
					if (auto* geometry = glowGeometryNode->AsGeometry()) {
						shaderProperty = RE::NiPointer<RE::BSEffectShaderProperty>(
							skyrim_cast<RE::BSEffectShaderProperty*>(geometry->GetGeometryRuntimeData().shaderProperty.get()));
					}
				}
				if (!shaderProperty) {
					logs::warn("Animation::WeaponImpactVFX: nodo \"{}\" sin BSEffectShaderProperty -- sin pulso de brillo.", Constants::kImpactGlowGeometryNodeName);
				}

				logs::info("Animation::WeaponImpactVFX: colocado en ({:.1f},{:.1f},{:.1f}), glowNode={}, shaderProperty={}.",
					a_position.x, a_position.y, a_position.z, glowNode != nullptr, shaderProperty != nullptr);

				StartTicking(a_handle, glowNode, std::move(shaderProperty));
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::WeaponImpactVFX: el 3D del VFX de impacto nunca llegó a cargar, se aborta.");
				return;
			}

			std::thread([a_handle, a_position, a_attemptsLeft]() mutable {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_handle, a_position, a_attemptsLeft]() mutable {
					WaitFor3DThenStart(a_handle, a_position, a_attemptsLeft - 1);
				});
			}).detach();
		}
	}

	void SpawnImpactVFX(RE::TESObjectREFR& a_spawnAt, const RE::NiPoint3& a_position)
	{
		auto* form = GetImpactActivatorForm();
		if (!form) {
			return;
		}

		auto ref = a_spawnAt.PlaceObjectAtMe(form, false);
		if (!ref) {
			logs::warn("Animation::SpawnImpactVFX: PlaceObjectAtMe devolvió nullptr.");
			return;
		}

		// Igual que Physics::SpawnReplica/Animation::WeaponVFX/
		// Animation::WeaponGlow: sin esto, el jugador podría activarlo/
		// recogerlo con la tecla de activar.
		ref->SetActivationBlocked(true);

		const auto handle = RE::ObjectRefHandle(ref.get());
		WaitFor3DThenStart(handle, a_position, kMax3DWaitAttempts);

		// Autodestrucción diferida -- mismo patrón que
		// Animation::FadeOutMovementVFX, sin generación (ver el header):
		// nada más apunta nunca a este handle, así que no hay ningún
		// cierre en marcha que este pueda pisar por error.
		std::thread([handle]() {
			std::this_thread::sleep_for(Constants::kImpactVfxLifetime);
			SKSE::GetTaskInterface()->AddTask([handle]() {
				Physics::DestroyReplica(handle);
			});
		}).detach();
	}
}
