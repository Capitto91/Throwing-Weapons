// Implementación del VFX de movimiento (chispas). Ver el header para el
// porqué del bucle de tick manual (tercer intento, tras BSTempEffectParticle
// y NiNode::AttachChild, ninguno de los dos renderizaba nada).

#include "8.- ANIMATION/WeaponVFX.h"

#include "1.- CORE/Constants.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <atomic>
#include <thread>

namespace Animation
{
	namespace
	{
		// Mismo margen que Physics::SpawnReplica -- ~800ms de sobra para lo
		// que suele tardar el 3D de una referencia recién colocada en
		// cargar en segundo plano.
		constexpr int kMax3DWaitAttempts = 50;

		RE::TESBoundObject* g_activatorForm = nullptr;
		bool                g_activatorLookupDone = false;

		// Resuelto una sola vez por sesión (el formulario no cambia entre
		// llamadas) -- FormID local ya enmascarado a 12 bits (ver
		// Constants::kMovementVfxActivatorLocalFormID).
		RE::TESBoundObject* GetActivatorForm()
		{
			if (!g_activatorLookupDone) {
				g_activatorLookupDone = true;
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					g_activatorForm = dataHandler->LookupForm<RE::TESObjectACTI>(Constants::kMovementVfxActivatorLocalFormID, Constants::kSoundPluginName);
				}
				if (!g_activatorForm) {
					logs::warn("Animation::WeaponVFX: no se encontró el Activator (FormID local 0x{:03X}) en \"{}\".",
						Constants::kMovementVfxActivatorLocalFormID, Constants::kSoundPluginName);
				}
			}
			return g_activatorForm;
		}

		// VFX activo en este instante, si lo hay.
		RE::ObjectRefHandle g_activeVfxHandle;
		Physics::TickToken  g_tickToken;

		// Incrementada en cada Start/Stop -- el reintento de espera de 3D
		// (WaitFor3DThenStartTicking) captura la generación vigente y la
		// comprueba de nuevo antes de actuar, para descartarse en silencio
		// si un Start/Stop posterior ya decidió el destino del VFX
		// mientras dormía. Mismo patrón ya usado en los dos intentos
		// anteriores de este mismo archivo.
		std::atomic<std::uint64_t> g_generation{ 0 };

		// Sigue cada tick la posición mundial actual de a_getTargetPosition
		// (el hueso "WEAPON", o el nodo raíz de la réplica -- reevaluado en
		// cada tick, no una foto fija del instante de arranque, para que el
		// VFX seguía moviéndose con su objetivo) -- mismo patrón de control
		// manual ya usado en todo el proyecto para la réplica del arma
		// (SetPosition + Physics::SyncHavok + Update3DPosition, ver
		// Physics::SpawnReplica/StartTickLoop). El propio bucle nunca se
		// autodetiene (siempre devuelve true) -- el único punto de parada
		// es StopMovementVFX, vía Physics::CancelTickLoop.
		void StartTicking(RE::ObjectRefHandle a_vfxHandle, std::function<RE::NiPoint3()> a_getTargetPosition)
		{
			auto  vfxRef = a_vfxHandle.get();
			auto* node3D = vfxRef ? vfxRef->Get3D() : nullptr;
			if (!node3D) {
				return;
			}

			// Modo Havok "movido por código" -- mismo motivo que
			// Physics::SpawnReplica: sin esto, forzar la posición de un
			// cuerpo simulado activamente produce tirones/clipping.
			node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);

			// Mismo mecanismo que el script vanilla FXSetBlendVariableScript
			// (Papyrus, OnLoad -> SetAnimationVariableFloat), pero llamado
			// directamente en C++ sobre la referencia real recién colocada
			// -- TESObjectREFR hereda IAnimationGraphManagerHolder, así que
			// esta llamada es válida (a diferencia del intento con
			// BSTempEffectParticle, que no tenía grafo de animación
			// propio). Sin esto, el NiControllerManager del .nif nunca
			// recibe ninguna señal de a qué secuencia (partA/partB) cruzar.
			const bool blendSet = vfxRef->SetGraphVariableFloat(Constants::kMovementVfxToggleBlendVariable, Constants::kMovementVfxToggleBlendValue);
			logs::info("Animation::WeaponVFX: SetGraphVariableFloat(\"{}\", {})={}.", Constants::kMovementVfxToggleBlendVariable, Constants::kMovementVfxToggleBlendValue, blendSet);

			g_activeVfxHandle = a_vfxHandle;
			g_tickToken = Physics::StartTickLoop(a_vfxHandle, [getPos = std::move(a_getTargetPosition)](RE::TESObjectREFR& a_refr, float) {
				const auto pos = getPos();
				a_refr.SetPosition(pos);
				Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
				return true;
			});

			logs::info("Animation::WeaponVFX: siguiendo por tick, posición inicial ({:.1f},{:.1f},{:.1f}).", node3D->world.translate.x, node3D->world.translate.y, node3D->world.translate.z);
		}

		void WaitFor3DThenStartTicking(RE::ObjectRefHandle a_vfxHandle, std::function<RE::NiPoint3()> a_getTargetPosition, int a_attemptsLeft, std::uint64_t a_generation)
		{
			if (g_generation.load() != a_generation) {
				return;
			}

			auto vfxRef = a_vfxHandle.get();
			if (!vfxRef) {
				return;
			}

			if (vfxRef->Get3D()) {
				StartTicking(a_vfxHandle, std::move(a_getTargetPosition));
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::WeaponVFX: el 3D del VFX nunca llegó a cargar, se aborta.");
				return;
			}

			std::thread([a_vfxHandle, getPos = std::move(a_getTargetPosition), a_attemptsLeft, a_generation]() mutable {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_vfxHandle, getPos = std::move(getPos), a_attemptsLeft, a_generation]() mutable {
					WaitFor3DThenStartTicking(a_vfxHandle, std::move(getPos), a_attemptsLeft - 1, a_generation);
				});
			}).detach();
		}

		// Común a StartMovementVFXOnActor/OnReplica: coloca a_form en
		// a_spawnAt y espera su 3D antes de arrancar el seguimiento por
		// tick sobre a_getTargetPosition.
		void StartOn(RE::TESObjectREFR& a_spawnAt, std::function<RE::NiPoint3()> a_getTargetPosition, RE::TESBoundObject* a_form)
		{
			if (!a_form) {
				return;
			}

			auto ref = a_spawnAt.PlaceObjectAtMe(a_form, false);
			if (!ref) {
				logs::warn("Animation::WeaponVFX: PlaceObjectAtMe devolvió nullptr.");
				return;
			}

			// Igual que la réplica del arma (Physics::SpawnReplica): sin
			// esto, el jugador podría activarlo/recogerlo con la tecla de
			// activar.
			ref->SetActivationBlocked(true);

			const auto generation = ++g_generation;
			WaitFor3DThenStartTicking(RE::ObjectRefHandle(ref.get()), std::move(a_getTargetPosition), kMax3DWaitAttempts, generation);
		}
	}

	void StartMovementVFXOnActor(RE::Actor& a_actor)
	{
		StopMovementVFX();

		auto* handNode = a_actor.GetNodeByName("WEAPON");
		if (!handNode) {
			logs::warn("Animation::StartMovementVFXOnActor: hueso \"WEAPON\" no encontrado.");
			return;
		}

		// El propio jugador es un singleton estable durante toda la sesión
		// (WeaponManager solo llama a esta función con RE::PlayerCharacter::
		// GetSingleton()) -- reevaluado directamente cada tick, sin
		// necesidad de convertir a_actor a un handle propio.
		auto getPos = []() -> RE::NiPoint3 {
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* node = player ? player->GetNodeByName("WEAPON") : nullptr;
			return node ? node->world.translate : RE::NiPoint3{};
		};

		StartOn(a_actor, std::move(getPos), GetActivatorForm());
	}

	void StartMovementVFXOnReplica(RE::ObjectRefHandle a_handle)
	{
		StopMovementVFX();

		auto  replica = a_handle.get();
		auto* root = replica ? replica->Get3D() : nullptr;
		if (!replica || !root) {
			logs::warn("Animation::StartMovementVFXOnReplica: réplica sin 3D todavía.");
			return;
		}

		auto getPos = [handle = a_handle]() -> RE::NiPoint3 {
			auto replicaRef = handle.get();
			auto* replicaRoot = replicaRef ? replicaRef->Get3D() : nullptr;
			return replicaRoot ? replicaRoot->world.translate : RE::NiPoint3{};
		};

		StartOn(*replica, std::move(getPos), GetActivatorForm());
	}

	void StopMovementVFX()
	{
		++g_generation;

		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = {};

		if (g_activeVfxHandle) {
			Physics::DestroyReplica(g_activeVfxHandle);
			g_activeVfxHandle = {};
		}
	}

}
