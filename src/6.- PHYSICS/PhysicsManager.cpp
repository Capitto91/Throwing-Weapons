// Implementación del sistema físico.
// Actualiza la posición del arma aplicando las reglas de movimiento.

#include "6.- PHYSICS/PhysicsManager.h"

#include "1.- CORE/Constants.h"

#include <atomic>
#include <memory>
#include <thread>

namespace Physics
{
	namespace
	{
		// Margen de espera (en pasos de Constants::kTickInterval) para que
		// el 3D de una réplica recién creada termine de cargar en segundo
		// plano antes de darla por perdida. ~800ms de sobra para lo que en
		// la iteración anterior ha tardado en el juego (unos pocos frames).
		constexpr int kMax3DWaitAttempts = 50;

		void WaitFor3DThenReady(RE::ObjectRefHandle a_handle, int a_attemptsLeft, ReadyCallback a_onReady)
		{
			auto refr = a_handle.get();
			if (!refr) {
				a_onReady({});
				return;
			}

			if (auto* node3D = refr->Get3D()) {
				// Modo Havok "movido por código": deja de recibir fuerzas/
				// gravedad de la simulación pero conserva colisión — sin
				// esto, forzar la posición de un cuerpo simulado activamente
				// produce tirones/clipping (ver CLAUDE.md).
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

		// La réplica es un TESObjectWEAP real y activable: sin esto, el
		// jugador puede recogerla del suelo con la tecla de activar como
		// si fuera un arma suelta cualquiera, duplicando el arma (la
		// "real" sigue en su inventario, sin equipar, mientras el ciclo
		// dure) — comprobado en el juego.
		ref->SetActivationBlocked(true);

		WaitFor3DThenReady(RE::ObjectRefHandle(ref.get()), kMax3DWaitAttempts, std::move(a_onReady));
	}

	void SyncHavok(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_position, const RE::NiPoint3& a_angle)
	{
		// TESObjectREFR::SetPosition/SetAngle solo actualizan la posición/
		// rotación "lógica" del objeto; el bhkRigidBody de Havok, que sigue
		// existiendo aunque esté en modo kKeyframed, no se entera por sí
		// solo. bhkRigidBody trabaja en unidades de Havok (metros), de ahí
		// bhkWorld::GetWorldScale() para convertir desde unidades de juego.
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
		// El callback se guarda en el heap y se comparte por puntero (no se
		// copia en cada iteración): así, si el propio callback captura
		// estado mutable entre ticks (p. ej. distancia acumulada), ese
		// estado persiste correctamente de un tick al siguiente.
		auto callback = std::make_shared<TickCallback>(std::move(a_callback));
		auto active = std::make_shared<std::atomic<bool>>(true);

		std::thread([a_handle, callback, active]() {
			while (active->load()) {
				std::this_thread::sleep_for(Constants::kTickInterval);
				if (!active->load()) {
					return;
				}

				// Nunca se reencola llamando a AddTask desde dentro de la
				// propia tarea que se ejecuta: si esa cola no está separada
				// por fotogramas, encadenar así congela el juego por
				// completo (comprobado en la iteración anterior). El
				// reencolado real lo hace este hilo aparte, que solo
				// duerme y vuelve a pedir turno.
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
