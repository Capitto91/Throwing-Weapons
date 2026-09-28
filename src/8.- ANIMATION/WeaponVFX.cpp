// Chispas de movimiento -- ver WeaponVFX.h.

#include "8.- ANIMATION/WeaponVFX.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Settings.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <atomic>
#include <thread>

namespace Animation
{
	namespace
	{
		// Intentos de espera a que cargue el 3D (~800 ms).
		constexpr int kMax3DWaitAttempts = 50;

		// Activator continuo o "de un solo uso", resuelto una vez por sesión.
		RE::TESBoundObject* GetCachedActivatorForm(RE::FormID a_localFormID, RE::TESBoundObject*& a_cache, bool& a_lookupDone)
		{
			if (!a_lookupDone) {
				a_lookupDone = true;
				if (auto* dataHandler = RE::TESDataHandler::GetSingleton()) {
					a_cache = dataHandler->LookupForm<RE::TESObjectACTI>(a_localFormID, Constants::kSoundPluginName);
				}
				if (!a_cache) {
					logs::warn("Animation::WeaponVFX: no se encontró el Activator (FormID local 0x{:03X}) en \"{}\".",
						a_localFormID, Constants::kSoundPluginName);
				}
			}
			return a_cache;
		}

		RE::TESBoundObject* GetOnActivatorForm()
		{
			static RE::TESBoundObject* cache = nullptr;
			static bool                lookupDone = false;
			return GetCachedActivatorForm(Constants::kMovementVfxActivatorLocalFormID, cache, lookupDone);
		}

		RE::TESBoundObject* GetOffActivatorForm()
		{
			static RE::TESBoundObject* cache = nullptr;
			static bool                lookupDone = false;
			return GetCachedActivatorForm(Constants::kMovementVfxOffActivatorLocalFormID, cache, lookupDone);
		}

		// VFX activo ahora: el continuo o el "de un solo uso" que se está apagando.
		RE::ObjectRefHandle g_activeVfxHandle;
		Physics::TickToken  g_tickToken;      // sigue la posición del objetivo (mano/réplica) -- solo el continuo la usa
		Physics::TickToken  g_activateToken;  // reintenta Activate() hasta que tenga éxito -- ver StartActivatingSequence

		// true si el activo es el "de un solo uso": FadeOutMovementVFX no coloca otro.
		bool g_isBurstActive = false;

		// Generación: las esperas de 3D se descartan si otro Start/Stop llegó antes.
		std::atomic<std::uint64_t> g_generation{ 0 };

		// NiControllerManager del nodo raíz del VFX.
		RE::NiControllerManager* GetVfxControllerManager(RE::ObjectRefHandle a_handle)
		{
			auto  ref = a_handle.get();
			auto* root = ref ? ref->Get3D() : nullptr;
			return root ? root->GetController<RE::NiControllerManager>() : nullptr;
		}

		// Activa la secuencia Constants::kMovementVfxSequenceName cada tick hasta que Animating().
		// a_onActivated se llama una vez al confirmarse que se reproduce.
		void StartActivatingSequence(RE::ObjectRefHandle a_handle, std::function<void()> a_onActivated = {})
		{
			Physics::CancelTickLoop(g_activateToken);
			g_activateToken = Physics::StartTickLoop(a_handle, [onActivated = std::move(a_onActivated)](RE::TESObjectREFR& a_refr, float) {
				auto* manager = a_refr.Get3D() ? a_refr.Get3D()->GetController<RE::NiControllerManager>() : nullptr;
				auto* sequence = manager ? manager->GetSequenceByName(Constants::kMovementVfxSequenceName) : nullptr;

				if (sequence && sequence->Animating()) {
					if (onActivated) {
						onActivated();
					}
					return false;
				}

				const bool activated = sequence && sequence->Activate(0, false, 1.0f, 0.0f, nullptr, false);
				if (activated && onActivated) {
					onActivated();
				}
				return !activated;
			});
		}

		// Sigue cada tick la posición de a_getTargetPosition; solo lo para StopMovementVFX.
		void StartTicking(RE::ObjectRefHandle a_vfxHandle, std::function<RE::NiPoint3()> a_getTargetPosition, std::function<void()> a_onReady)
		{
			auto  vfxRef = a_vfxHandle.get();
			auto* node3D = vfxRef ? vfxRef->Get3D() : nullptr;
			if (!node3D) {
				return;
			}

			// Movido por código: sin fuerzas ni gravedad.
			node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);

			StartActivatingSequence(a_vfxHandle, std::move(a_onReady));

			// g_activeVfxHandle ya lo fijó StartOn al colocar el Activator.
			g_tickToken = Physics::StartTickLoop(a_vfxHandle, [getPos = std::move(a_getTargetPosition)](RE::TESObjectREFR& a_refr, float) {
				const auto pos = getPos();
				a_refr.SetPosition(pos);
				Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
				return true;
			});

		}

		void WaitFor3DThenStartTicking(RE::ObjectRefHandle a_vfxHandle, std::function<RE::NiPoint3()> a_getTargetPosition, int a_attemptsLeft, std::uint64_t a_generation, std::function<void()> a_onReady)
		{
			if (g_generation.load() != a_generation) {
				return;
			}

			auto vfxRef = a_vfxHandle.get();
			if (!vfxRef) {
				return;
			}

			if (vfxRef->Get3D()) {
				StartTicking(a_vfxHandle, std::move(a_getTargetPosition), std::move(a_onReady));
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::WeaponVFX: el 3D del VFX nunca llegó a cargar, se aborta.");
				return;
			}

			std::thread([a_vfxHandle, getPos = std::move(a_getTargetPosition), a_attemptsLeft, a_generation, onReady = std::move(a_onReady)]() mutable {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_vfxHandle, getPos = std::move(getPos), a_attemptsLeft, a_generation, onReady = std::move(onReady)]() mutable {
					WaitFor3DThenStartTicking(a_vfxHandle, std::move(getPos), a_attemptsLeft - 1, a_generation, std::move(onReady));
				});
			}).detach();
		}

		// Destruye el VFX saliente cuando han pasado kMovementVfxSwapOverlapDuration y el nuevo ya se ve.
		// kMovementVfxSwapSafetyTimeout lo destruye igualmente. Devuelve el aviso a pasar al nuevo.
		std::function<void()> ScheduleOldVfxSwap(RE::ObjectRefHandle a_oldHandle, Physics::TickToken a_oldTickToken, Physics::TickToken a_oldActivateToken)
		{
			struct SwapGate
			{
				std::atomic<bool> minOverlapElapsed{ false };
				std::atomic<bool> newVfxReady{ false };
				std::atomic<bool> destroyed{ false };
			};
			auto gate = std::make_shared<SwapGate>();

			auto destroyOld = [a_oldHandle, a_oldTickToken, a_oldActivateToken, gate]() {
				bool expected = false;
				if (gate->destroyed.compare_exchange_strong(expected, true)) {
					Physics::CancelTickLoop(a_oldTickToken);
					Physics::CancelTickLoop(a_oldActivateToken);
					Physics::DestroyReplica(a_oldHandle);
				}
			};

			auto tryDestroy = [gate, destroyOld]() {
				if (gate->minOverlapElapsed.load() && gate->newVfxReady.load()) {
					destroyOld();
				}
			};

			std::thread([gate, tryDestroy]() {
				std::this_thread::sleep_for(Constants::kMovementVfxSwapOverlapDuration);
				SKSE::GetTaskInterface()->AddTask([gate, tryDestroy]() {
					gate->minOverlapElapsed.store(true);
					tryDestroy();
				});
			}).detach();

			std::thread([destroyOld]() {
				std::this_thread::sleep_for(Constants::kMovementVfxSwapSafetyTimeout);
				SKSE::GetTaskInterface()->AddTask([destroyOld]() {
					destroyOld();
				});
			}).detach();

			return [gate, tryDestroy]() {
				gate->newVfxReady.store(true);
				tryDestroy();
			};
		}

		// Coloca a_form en a_spawnAt, espera su 3D y lo hace seguir a a_getTargetPosition,
		// solapando con el VFX anterior en vez de cortarlo.
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

			// Sin activación: el jugador no puede recogerlo.
			ref->SetActivationBlocked(true);

			auto oldHandle = g_activeVfxHandle;
			auto oldTickToken = g_tickToken;
			auto oldActivateToken = g_activateToken;

			// El nuevo pasa a ser el activo al colocarse, antes de que cargue su 3D.
			g_activeVfxHandle = RE::ObjectRefHandle(ref.get());
			g_tickToken = {};
			g_activateToken = {};

			// El nuevo es el continuo.
			g_isBurstActive = false;

			// El anterior se destruye al cumplirse las dos condiciones de ScheduleOldVfxSwap.
			auto onReady = ScheduleOldVfxSwap(oldHandle, oldTickToken, oldActivateToken);

			const auto generation = ++g_generation;
			WaitFor3DThenStartTicking(RE::ObjectRefHandle(ref.get()), std::move(a_getTargetPosition), kMax3DWaitAttempts, generation, std::move(onReady));
		}

		// Posición del hueso "WEAPON" del jugador, reevaluada cada tick.
		RE::NiPoint3 GetPlayerHandPosition()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* node = player ? player->GetNodeByName("WEAPON") : nullptr;
			return node ? node->world.translate : RE::NiPoint3{};
		}

		// Espera el 3D del "de un solo uso" en a_position (fijo) y activa su secuencia.
		void WaitFor3DThenStartBurst(RE::ObjectRefHandle a_handle, RE::NiPoint3 a_position, int a_attemptsLeft, std::uint64_t a_generation, std::function<void()> a_onReady)
		{
			if (g_generation.load() != a_generation) {
				return;
			}

			auto ref = a_handle.get();
			if (!ref) {
				return;
			}

			if (auto* node3D = ref->Get3D()) {
				node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);
				ref->SetPosition(a_position);
				Physics::SyncHavok(*ref, a_position, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
				StartActivatingSequence(a_handle, std::move(a_onReady));
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::WeaponVFX: el 3D del VFX \"de un solo uso\" nunca llegó a cargar, se aborta.");
				return;
			}

			std::thread([a_handle, a_position, a_attemptsLeft, a_generation, onReady = std::move(a_onReady)]() mutable {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_handle, a_position, a_attemptsLeft, a_generation, onReady = std::move(onReady)]() mutable {
					WaitFor3DThenStartBurst(a_handle, a_position, a_attemptsLeft - 1, a_generation, std::move(onReady));
				});
			}).detach();
		}
	}

	void StartMovementVFXOnActor(RE::Actor& a_actor, bool a_checkSetting)
	{
		// Desactivable con [VFX] Particles, salvo a_checkSetting=false (power attacks).
		if (a_checkSetting && !Settings::GetParticles()) {
			return;
		}

		auto* handNode = a_actor.GetNodeByName("WEAPON");
		if (!handNode) {
			logs::warn("Animation::StartMovementVFXOnActor: hueso \"WEAPON\" no encontrado.");
			return;
		}

		StartOn(a_actor, GetPlayerHandPosition, GetOnActivatorForm());
	}

	void RetargetMovementVFXToActor(RE::Actor& a_actor)
	{
		if (!g_activeVfxHandle) {
			return;
		}

		if (!a_actor.GetNodeByName("WEAPON")) {
			logs::warn("Animation::RetargetMovementVFXToActor: hueso \"WEAPON\" no encontrado.");
			return;
		}

		// Mismo Activator: solo cambia qué posición sigue; se cancela el bucle anterior.
		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = Physics::StartTickLoop(g_activeVfxHandle, [](RE::TESObjectREFR& a_refr, float) {
			const auto pos = GetPlayerHandPosition();
			a_refr.SetPosition(pos);
			Physics::SyncHavok(a_refr, pos, RE::NiPoint3{ 0.0f, 0.0f, 0.0f });
			return true;
		});
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
			auto replicaRef = handle.get();
			auto* replicaRoot = replicaRef ? replicaRef->Get3D() : nullptr;
			if (replicaRoot) {
				lastPosition = replicaRoot->world.translate;
			}
			return lastPosition;
		};

		StartOn(*replica, std::move(getPos), GetOnActivatorForm());
	}

	void StopMovementVFX()
	{
		++g_generation;

		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = {};
		Physics::CancelTickLoop(g_activateToken);
		g_activateToken = {};

		// Único punto que limpia g_activeVfxHandle.
		if (g_activeVfxHandle) {
			Physics::DestroyReplica(g_activeVfxHandle);
			g_activeVfxHandle = {};
		}

		g_isBurstActive = false;
	}

	void FadeOutMovementVFX(bool a_extraSettleDelay)
	{
		if (!g_activeVfxHandle) {
			return;
		}

		// Espera kCatchVfxSettleDelay y se vuelve a llamar; se descarta si otro VFX la releva.
		if (a_extraSettleDelay) {
			const auto generation = g_generation.load();
			std::thread([generation]() {
				std::this_thread::sleep_for(Constants::kCatchVfxSettleDelay);
				SKSE::GetTaskInterface()->AddTask([generation]() {
					if (g_generation.load() == generation) {
						FadeOutMovementVFX(false);
					}
				});
			}).detach();
			return;
		}

		// Ya se está apagando: no se coloca otro.
		if (g_isBurstActive) {
			return;
		}

		// Congela el continuo donde está.
		Physics::CancelTickLoop(g_tickToken);
		g_tickToken = {};
		Physics::CancelTickLoop(g_activateToken);
		g_activateToken = {};

		auto  oldHandle = g_activeVfxHandle;
		auto  oldRef = oldHandle.get();
		auto* oldRoot = oldRef ? oldRef->Get3D() : nullptr;
		if (!oldRef || !oldRoot) {
			// Sin 3D: se borra el continuo y nada más.
			StopMovementVFX();
			return;
		}
		const RE::NiPoint3 position = oldRoot->world.translate;

		// Coloca el "de un solo uso" donde estaba el continuo; se apaga solo.
		auto burstRef = oldRef->PlaceObjectAtMe(GetOffActivatorForm(), false);
		if (!burstRef) {
			logs::warn("Animation::FadeOutMovementVFX: PlaceObjectAtMe del \"de un solo uso\" devolvió nullptr -- corte inmediato de reserva.");
			StopMovementVFX();
			return;
		}
		burstRef->SetActivationBlocked(true);

		const auto generation = ++g_generation;
		g_activeVfxHandle = RE::ObjectRefHandle(burstRef.get());
		g_isBurstActive = true;


		// El continuo se destruye con ScheduleOldVfxSwap (independiente de la generación).
		auto onReady = ScheduleOldVfxSwap(oldHandle, {}, {});

		WaitFor3DThenStartBurst(g_activeVfxHandle, position, kMax3DWaitAttempts, generation, std::move(onReady));

		// Borra el "de un solo uso" pasado kMovementVfxFadeOutSafetyMargin, salvo que se relevara antes.
		std::thread([generation]() {
			std::this_thread::sleep_for(Constants::kMovementVfxFadeOutSafetyMargin);
			SKSE::GetTaskInterface()->AddTask([generation]() {
				if (g_generation.load() == generation) {
					StopMovementVFX();
				}
			});
		}).detach();
	}
}
