// Réplica y bucle de tick -- ver PhysicsManager.h.

#include "6.- PHYSICS/PhysicsManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/FrameHook.h"
#include "1.- CORE/Scheduler.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace Physics
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// Intentos de espera a que cargue el 3D (~800 ms).
		constexpr int kMax3DWaitAttempts = 50;

		// Bucle de StartTickLoop movido por RunFrame.
		struct FrameLoop
		{
			RE::ObjectRefHandle           handle;
			std::shared_ptr<TickCallback> callback;
			TickToken                     active;
		};

		// Bucles recién creados; RunFrame los incorpora al empezar el fotograma siguiente.
		std::mutex             g_pendingLock;
		std::vector<FrameLoop> g_pendingLoops;

		// Bucles en marcha; solo los toca RunFrame (hilo principal).
		std::vector<FrameLoop> g_frameLoops;

		// Segundos reales desde a_lastTick (que pasa a ser ahora), con tope kMaxTickDeltaSeconds.
		// Paso de los bucles con hilos, sin FrameHook.
		float ConsumeRealDelta(Clock::time_point& a_lastTick)
		{
			const auto  now = Clock::now();
			const float delta = std::chrono::duration<float>(now - a_lastTick).count();
			a_lastTick = now;
			return (std::min)(delta, Constants::kMaxTickDeltaSeconds);
		}

		// Un intento de WaitFor3D; si el 3D no está, se reprograma con Scheduler (en el hilo principal).
		void PollFor3D(RE::ObjectRefHandle a_handle, const char* a_what, int a_attemptsLeft, ReadyCallback a_onReady)
		{
			auto refr = a_handle.get();
			if (!refr) {
				a_onReady({});
				return;
			}

			if (refr->Get3D()) {
				a_onReady(a_handle);
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Physics::WaitFor3D: el 3D de {} nunca llegó a cargar, se aborta.", a_what);
				a_onReady({});
				return;
			}

			(void)Scheduler::After(Constants::kTickInterval, [a_handle, a_what, a_attemptsLeft, onReady = std::move(a_onReady)]() mutable {
				PollFor3D(a_handle, a_what, a_attemptsLeft - 1, std::move(onReady));
			});
		}
	}

	void WaitFor3D(RE::ObjectRefHandle a_handle, const char* a_what, ReadyCallback a_onReady)
	{
		PollFor3D(a_handle, a_what, kMax3DWaitAttempts, std::move(a_onReady));
	}

	void SpawnReplica(RE::Actor* a_actor, RE::TESObjectWEAP* a_weapon, const RE::NiPoint3& a_position, ReadyCallback a_onReady)
	{
		if (!a_actor || !a_weapon) {
			a_onReady({});
			return;
		}

		auto ref = a_actor->PlaceObjectAtMe(a_weapon, false);
		if (!ref) {
			a_onReady({});
			return;
		}

		ref->SetPosition(a_position);

		// Sin activación: el jugador no puede recogerla del suelo.
		ref->SetActivationBlocked(true);

		WaitFor3D(RE::ObjectRefHandle(ref.get()), "la réplica", [onReady = std::move(a_onReady)](RE::ObjectRefHandle a_handle) {
			auto  refr = a_handle.get();
			auto* node3D = refr ? refr->Get3D() : nullptr;
			if (node3D) {
				// Movida por código: sin fuerzas ni gravedad, con colisión.
				node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);
				SyncHavok(*refr, refr->GetPosition(), refr->GetAngle());
			}
			onReady(node3D ? a_handle : RE::ObjectRefHandle{});
		});
	}

	void SyncHavok(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_position, const RE::NiPoint3& a_angle)
	{
		// SetPosition/SetAngle no mueven el bhkRigidBody: se escribe aparte, en unidades de Havok.
		auto* node = a_refr.Get3D();
		auto* collisionObj = node ? node->GetCollisionObject() : nullptr;
		auto* rigidBody = collisionObj ? collisionObj->GetRigidBody() : nullptr;

		if (rigidBody) {
			RE::hkVector4 havokPosition(a_position * RE::bhkWorld::GetWorldScale());
			rigidBody->SetPosition(havokPosition);

			RE::NiMatrix3 rotationMatrix;
			rotationMatrix.EulerAnglesToAxesZXY(a_angle);
			const RE::NiQuaternion niRotation(rotationMatrix);

			RE::hkQuaternion havokRotation;
			havokRotation.vec = RE::hkVector4(niRotation.x, niRotation.y, niRotation.z, niRotation.w);
			rigidBody->SetRotation(havokRotation);
		}

		a_refr.Update3DPosition(true);
	}

	void MoveTo(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_position)
	{
		a_refr.SetPosition(a_position);
		SyncHavok(a_refr, a_position, a_refr.GetAngle());
	}

	TickToken StartTickLoop(RE::ObjectRefHandle a_handle, TickCallback a_callback)
	{
		// El callback se comparte por puntero para conservar su estado entre ticks.
		auto callback = std::make_shared<TickCallback>(std::move(a_callback));
		auto active = std::make_shared<std::atomic<bool>>(true);

		if (FrameHook::IsInstalled()) {
			std::scoped_lock lock(g_pendingLock);
			g_pendingLoops.push_back(FrameLoop{ a_handle, std::move(callback), active });
			return active;
		}

		// Sin FrameHook: hilo que duerme kTickInterval y reencola con el tiempo real transcurrido.
		auto lastTick = std::make_shared<Clock::time_point>(Clock::now());

		std::thread([a_handle, callback, active, lastTick]() {
			while (active->load()) {
				std::this_thread::sleep_for(Constants::kTickInterval);
				if (!active->load()) {
					return;
				}

				// Este hilo aparte es quien reencola; nunca una tarea a sí misma.
				SKSE::GetTaskInterface()->AddTask([a_handle, callback, active, lastTick]() {
					if (!active->load()) {
						return;
					}

					auto refr = a_handle.get();
					if (!refr || !(*callback)(*refr, ConsumeRealDelta(*lastTick))) {
						active->store(false);
					}
				});
			}
		}).detach();

		return active;
	}

	void RunFrame(float a_deltaSeconds)
	{
		{
			std::scoped_lock lock(g_pendingLock);
			for (auto& loop : g_pendingLoops) {
				g_frameLoops.push_back(std::move(loop));
			}
			g_pendingLoops.clear();
		}

		const float delta = (std::min)(a_deltaSeconds, Constants::kMaxTickDeltaSeconds);

		// Un callback puede crear bucles (van a g_pendingLoops) o cancelar otros (su token).
		for (auto& loop : g_frameLoops) {
			if (!loop.active->load()) {
				continue;
			}

			auto refr = loop.handle.get();
			if (!refr) {
				loop.active->store(false);
				continue;
			}

			// Un callback que lanza se detiene solo, sin afectar a los demás bucles.
			try {
				if (!(*loop.callback)(*refr, delta)) {
					loop.active->store(false);
				}
			} catch (const std::exception& e) {
				logs::error("Physics::RunFrame: excepción en un bucle, se detiene: {}", e.what());
				loop.active->store(false);
			} catch (...) {
				logs::error("Physics::RunFrame: excepción desconocida en un bucle, se detiene.");
				loop.active->store(false);
			}
		}

		std::erase_if(g_frameLoops, [](const FrameLoop& a_loop) { return !a_loop.active->load(); });
	}

	std::size_t GetActiveLoopCount()
	{
		std::scoped_lock lock(g_pendingLock);
		return g_frameLoops.size() + g_pendingLoops.size();
	}

	void CancelTickLoop(TickToken& a_token)
	{
		if (a_token) {
			a_token->store(false);
			a_token.reset();
		}
	}

	void DestroyReference(RE::ObjectRefHandle a_handle)
	{
		if (auto refr = a_handle.get()) {
			refr->Disable();
			refr->SetDelete(true);
		}
	}
}
