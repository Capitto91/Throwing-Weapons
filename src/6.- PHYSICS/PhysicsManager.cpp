// Réplica y bucle de tick -- ver PhysicsManager.h.

#include "6.- PHYSICS/PhysicsManager.h"

#include "1.- CORE/Constants.h"

#include <atomic>
#include <memory>
#include <thread>

namespace Physics
{
	namespace
	{
		// Intentos de espera a que cargue el 3D (~800 ms).
		constexpr int kMax3DWaitAttempts = 50;

		void WaitFor3DThenReady(RE::ObjectRefHandle a_handle, int a_attemptsLeft, ReadyCallback a_onReady)
		{
			auto refr = a_handle.get();
			if (!refr) {
				a_onReady({});
				return;
			}

			if (auto* node3D = refr->Get3D()) {
				// Movida por código: sin fuerzas ni gravedad, con colisión.
				node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);
				SyncHavok(*refr, refr->GetPosition(), refr->GetAngle());
				a_onReady(a_handle);
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Physics::SpawnReplica: agotados los reintentos, el 3D nunca llegó a cargar.");
				a_onReady({});
				return;
			}

			std::thread([a_handle, a_attemptsLeft, onReady = std::move(a_onReady)]() mutable {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_handle, a_attemptsLeft, onReady = std::move(onReady)]() mutable {
					WaitFor3DThenReady(a_handle, a_attemptsLeft - 1, std::move(onReady));
				});
			}).detach();
		}
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

		WaitFor3DThenReady(RE::ObjectRefHandle(ref.get()), kMax3DWaitAttempts, std::move(a_onReady));
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

	TickToken StartTickLoop(RE::ObjectRefHandle a_handle, TickCallback a_callback)
	{
		// El callback se comparte por puntero para conservar su estado entre ticks.
		auto callback = std::make_shared<TickCallback>(std::move(a_callback));
		auto active = std::make_shared<std::atomic<bool>>(true);

		std::thread([a_handle, callback, active]() {
			while (active->load()) {
				std::this_thread::sleep_for(Constants::kTickInterval);
				if (!active->load()) {
					return;
				}

				// Este hilo aparte es quien reencola; nunca una tarea a sí misma.
				SKSE::GetTaskInterface()->AddTask([a_handle, callback, active]() {
					if (!active->load()) {
						return;
					}

					auto refr = a_handle.get();
					if (!refr || !(*callback)(*refr, Constants::kTickDeltaSeconds)) {
						active->store(false);
					}
				});
			}
		}).detach();

		return active;
	}

	void CancelTickLoop(const TickToken& a_token)
	{
		if (a_token) {
			a_token->store(false);
		}
	}

	void DestroyReplica(RE::ObjectRefHandle a_handle)
	{
		if (auto refr = a_handle.get()) {
			refr->Disable();
			refr->SetDelete(true);
		}
	}
}
