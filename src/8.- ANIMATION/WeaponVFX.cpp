// Chispas de movimiento -- ver WeaponVFX.h.

#include "8.- ANIMATION/WeaponVFX.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "1.- CORE/Settings.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <atomic>

namespace Animation
{
	namespace
	{
		// Clase (RTTI) del controlador que hace nacer las partículas, en la cadena del NiParticleSystem.
		constexpr std::string_view kEmitterControllerRTTIName{ "NiPSysEmitterCtlr" };

		// Efecto de chispas; el bucle lo marca retirado cuando el motor ya lo va a borrar.
		struct SparksEffect
		{
			RE::NiPointer<RE::BSTempEffectParticle> particle;
			std::atomic<bool>                       retired{ false };
		};

		std::shared_ptr<SparksEffect> g_sparks;
		std::function<RE::NiPoint3()> g_getTargetPosition;  // posición que siguen las chispas
		bool                          g_fadingOut = false;  // emisión parada, esperando a que mueran las partículas
		Physics::TickToken            g_tickToken;
		Scheduler::CancelToken        g_fadeOutToken;       // apagado diferido de FadeOutMovementVFX(true)

		// Posición del hueso "WEAPON" del jugador, reevaluada cada tick.
		RE::NiPoint3 GetPlayerHandPosition()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* node = player ? player->GetNodeByName("WEAPON") : nullptr;
			return node ? node->world.translate : RE::NiPoint3{};
		}

		// Transformación local de a_node que lo deja en a_worldTransform (mismo cálculo que WeaponTrail).
		RE::NiTransform GetLocalTransform(RE::NiAVObject* a_node, const RE::NiTransform& a_worldTransform)
		{
			if (auto* parent = a_node->parent) {
				return parent->world.Invert() * a_worldTransform;
			}

			return a_worldTransform;
		}

		// Hace que el motor retire el efecto en su próxima actualización (age >= lifetime).
		void Retire(SparksEffect& a_sparks)
		{
			a_sparks.retired = true;
			if (a_sparks.particle) {
				a_sparks.particle->age = a_sparks.particle->lifetime;
			}
		}

		// Activa o desactiva el NiPSysEmitterCtlr de cada sistema de partículas bajo a_root.
		// Devuelve cuántas partículas siguen vivas en total.
		std::uint32_t SetEmissionAndCountParticles(RE::NiAVObject* a_root, bool a_emitting)
		{
			std::uint32_t liveParticles = 0;

			RE::BSVisit::TraverseScenegraphGeometries(a_root, [&](RE::BSGeometry* a_geometry) {
				auto* particles = a_geometry->AsParticlesGeom();
				if (!particles) {
					return RE::BSVisit::BSVisitControl::kContinue;
				}

				for (auto* controller = particles->GetControllers(); controller; controller = controller->GetNext()) {
					const auto* rtti = controller->GetRTTI();
					if (rtti && rtti->GetName() && rtti->GetName() == kEmitterControllerRTTIName) {
						controller->flags.set(a_emitting, RE::NiTimeController::Flag::kActive);
					}
				}

				if (const auto& data = particles->GetParticlesRuntimeData().particleData) {
					liveParticles += data->GetActiveVertexCount();
				}
				return RE::BSVisit::BSVisitControl::kContinue;
			});

			return liveParticles;
		}

		// Bucle por fotograma: lleva el nodo Constants::kMovementVfxAnchorNodeName a a_getTargetPosition y
		// aplica a_emitting; sin emisión, retira el efecto en cuanto no quedan partículas vivas.
		void StartTicking(std::shared_ptr<SparksEffect> a_sparks, std::function<RE::NiPoint3()> a_getTargetPosition, bool a_emitting)
		{
			Physics::CancelTickLoop(g_tickToken);
			g_tickToken = {};

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !a_sparks) {
				return;
			}

			// Atado al jugador, que siempre existe: el efecto no es una referencia.
			g_tickToken = Physics::StartTickLoop(player->GetHandle(), [sparks = std::move(a_sparks), getPos = std::move(a_getTargetPosition), a_emitting, fadeOutSeconds = 0.0f, warned = false](RE::TESObjectREFR&, float a_deltaSeconds) mutable {
				auto& particle = sparks->particle;

				// Vida reiniciada cada tick: el motor solo lo retira si este bucle deja de correr.
				particle->age = 0.0f;

				auto* root = particle->particleObject.get();
				if (!root) {
					// Apagado antes de que cargue el 3D: no hay partículas que esperar.
					if (!a_emitting) {
						Retire(*sparks);
						return false;
					}
					return true;
				}

				if (auto* fadeNode = root->AsFadeNode()) {
					fadeNode->GetRuntimeData().currentFade = 1.0f;
				}

				if (auto* anchor = root->GetObjectByName(Constants::kMovementVfxAnchorNodeName)) {
					RE::NiTransform worldTransform = anchor->world;
					worldTransform.translate = getPos();
					anchor->local = GetLocalTransform(anchor, worldTransform);
					anchor->world = worldTransform;
				} else if (!warned) {
					warned = true;
					logs::warn("Animation::WeaponVFX: el efecto '{}' no tiene el nodo '{}'.", Constants::kMovementVfxEffectPath, Constants::kMovementVfxAnchorNodeName);
				}

				const auto liveParticles = SetEmissionAndCountParticles(root, a_emitting);
				if (a_emitting) {
					return true;
				}

				fadeOutSeconds += a_deltaSeconds;
				if (liveParticles == 0) {
					Retire(*sparks);
					return false;
				}

				if (fadeOutSeconds >= Constants::kMovementVfxFadeOutSafetySeconds) {
					logs::warn("Animation::WeaponVFX: quedan {} partículas tras {:.1f} s sin emisión, se retira el efecto.", liveParticles, fadeOutSeconds);
					Retire(*sparks);
					return false;
				}

				return true;
			});
		}

		// Corta las chispas de golpe.
		void StopNow()
		{
			Scheduler::Cancel(g_fadeOutToken);
			g_fadeOutToken = {};
			Physics::CancelTickLoop(g_tickToken);
			g_tickToken = {};

			if (g_sparks) {
				Retire(*g_sparks);
				g_sparks.reset();
			}

			g_getTargetPosition = {};
			g_fadingOut = false;
		}

		// Para la emisión; el bucle retira el efecto cuando mueren las partículas. No-op si ya se está apagando.
		void FadeOutNow()
		{
			if (!g_sparks || g_sparks->retired || g_fadingOut) {
				return;
			}

			g_fadingOut = true;
			StartTicking(g_sparks, g_getTargetPosition, false);
		}

		// Enciende las chispas siguiendo a_getTargetPosition. Reutiliza el efecto si sigue vivo en a_cell
		// (y reanuda su emisión si se estaba apagando); si no, crea uno nuevo.
		void Start(RE::TESObjectCELL* a_cell, std::function<RE::NiPoint3()> a_getTargetPosition)
		{
			Scheduler::Cancel(g_fadeOutToken);
			g_fadeOutToken = {};

			if (!a_cell) {
				logs::warn("Animation::WeaponVFX: sin celda, no se crea el efecto de chispas.");
				return;
			}

			if (!g_sparks || g_sparks->retired || g_sparks->particle->cell != a_cell) {
				StopNow();

				auto* particle = RE::BSTempEffectParticle::Spawn(a_cell, Constants::kMovementVfxEffectLifetime, Constants::kMovementVfxEffectPath, RE::NiMatrix3{}, a_getTargetPosition(), Constants::kMovementVfxScale, 7, nullptr);
				if (!particle) {
					logs::warn("Animation::WeaponVFX: BSTempEffectParticle::Spawn devolvió nullptr para '{}'.", Constants::kMovementVfxEffectPath);
					return;
				}

				g_sparks = std::make_shared<SparksEffect>();
				g_sparks->particle = RE::NiPointer<RE::BSTempEffectParticle>(particle);
			}

			g_getTargetPosition = std::move(a_getTargetPosition);
			g_fadingOut = false;
			StartTicking(g_sparks, g_getTargetPosition, true);
		}
	}

	void StartMovementVFXOnActor(RE::Actor& a_actor, bool a_checkSetting)
	{
		// Desactivable con [VFX] Particles, salvo a_checkSetting=false (power attacks).
		if (a_checkSetting && !Settings::GetParticles()) {
			return;
		}

		if (!a_actor.GetNodeByName("WEAPON")) {
			logs::warn("Animation::StartMovementVFXOnActor: hueso \"WEAPON\" no encontrado.");
			return;
		}

		Start(a_actor.GetParentCell(), GetPlayerHandPosition);
	}

	void RetargetMovementVFXToActor(RE::Actor& a_actor)
	{
		if (!g_sparks || g_sparks->retired) {
			return;
		}

		if (!a_actor.GetNodeByName("WEAPON")) {
			logs::warn("Animation::RetargetMovementVFXToActor: hueso \"WEAPON\" no encontrado.");
			return;
		}

		// Mismo efecto y mismo estado de emisión: solo cambia qué posición sigue.
		g_getTargetPosition = GetPlayerHandPosition;
		StartTicking(g_sparks, g_getTargetPosition, !g_fadingOut);
	}

	void StartMovementVFXOnReplica(RE::ObjectRefHandle a_handle)
	{
		// Desactivable con [VFX] Particles.
		if (!Settings::GetParticles()) {
			return;
		}

		auto  replica = a_handle.get();
		auto* root = replica ? replica->Get3D() : nullptr;
		if (!replica || !root) {
			logs::warn("Animation::StartMovementVFXOnReplica: réplica sin 3D todavía.");
			return;
		}

		// Si la réplica desaparece, sigue su última posición conocida.
		auto getPos = [handle = a_handle, lastPosition = root->world.translate]() mutable -> RE::NiPoint3 {
			auto  replicaRef = handle.get();
			auto* replicaRoot = replicaRef ? replicaRef->Get3D() : nullptr;
			if (replicaRoot) {
				lastPosition = replicaRoot->world.translate;
			}
			return lastPosition;
		};

		Start(replica->GetParentCell(), std::move(getPos));
	}

	void StopMovementVFX()
	{
		StopNow();
	}

	void FadeOutMovementVFX(bool a_extraSettleDelay)
	{
		if (!g_sparks) {
			return;
		}

		// Espera kCatchVfxSettleDelay; Start lo cancela si las chispas vuelven a encenderse antes.
		if (a_extraSettleDelay) {
			Scheduler::Cancel(g_fadeOutToken);
			g_fadeOutToken = Scheduler::After(Constants::kCatchVfxSettleDelay, [] { FadeOutNow(); });
			return;
		}

		FadeOutNow();
	}
}
