// Implementación del sistema de retorno.
// Controla velocidad, duración máxima del viaje y sincronización con la mano.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "3.- WEAPON/WeaponManager.h"
#include "5.- RETURN/ReturnTrajectory.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace Return
{
	namespace
	{
		// A diferencia del sondeo de Throw::TrackProjectile (una comprobación
		// cada ~50ms, un hilo nuevo por comprobación), aquí el movimiento debe
		// verse fluido: se usa un único hilo persistente que duerme un
		// intervalo corto y reencola en el hilo principal en bucle, en vez de
		// crear un hilo nuevo en cada paso.
		constexpr auto  kTickInterval = std::chrono::milliseconds(16);
		constexpr float kTickDeltaSeconds = 0.016f;

		// Margen de espera (en pasos de kTickInterval) para que el 3D de una
		// réplica recién creada termine de cargar en segundo plano antes de
		// darla por perdida (ver WaitFor3DThenStart). ~800ms de sobra para
		// lo que en el juego ha tardado unos pocos frames.
		constexpr int kMax3DWaitAttempts = 50;

		// Solo puede haber un regreso en marcha a la vez (arma arrojadiza
		// única); controla si el hilo de fondo sigue reencolando pasos.
		std::atomic<bool> g_active{ false };

		RE::NiPoint3 GetHandPosition(RE::Actor* a_player)
		{
			if (auto* weaponNode = a_player->GetNodeByName("WEAPON")) {
				return weaponNode->world.translate;
			}

			return a_player->GetPosition();
		}

		// TESObjectREFR::SetPosition solo actualiza la posición "lógica" del
		// objeto; el bhkRigidBody de Havok, que sigue existiendo aunque esté
		// en modo kKeyframed, no se entera por sí solo. bhkRigidBody trabaja
		// en unidades de Havok (metros), de ahí bhkWorld::GetWorldScale()
		// para convertir desde unidades de juego.
		void SetHavokPosition(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_position)
		{
			auto* node = a_refr.Get3D();
			auto* collisionObj = node ? node->GetCollisionObject() : nullptr;
			auto* rigidBody = collisionObj ? collisionObj->GetRigidBody() : nullptr;
			if (!rigidBody) {
				return;
			}

			RE::hkVector4 havokPosition(a_position * RE::bhkWorld::GetWorldScale());
			rigidBody->SetPosition(havokPosition);
		}

		// Detiene el hilo de fondo, borra la réplica controlada a mano y
		// notifica a WeaponManager para que reequipe el arma real. Comprueba
		// el estado antes de notificar por si el ciclo ya terminó por otra
		// vía (p. ej. recuperación instantánea al cerrar una pantalla de
		// carga a mitad del regreso), para no reequipar ni resetear el
		// estado dos veces.
		void FinishReturn(RE::ObjectRefHandle a_handle)
		{
			g_active = false;

			if (auto refr = a_handle.get()) {
				logs::info("Return::FinishReturn llegó a ({:.1f},{:.1f},{:.1f})", refr->GetPosition().x, refr->GetPosition().y, refr->GetPosition().z);
				refr->Disable();
				refr->SetDelete(true);
			}

			if (Weapon::WeaponManager::GetSingleton()->GetState() == Weapon::State::kReturning) {
				Weapon::WeaponManager::GetSingleton()->OnReturnComplete();
			}
		}

		void Tick(RE::ObjectRefHandle a_handle, RE::ActorHandle a_playerHandle)
		{
			if (Weapon::WeaponManager::GetSingleton()->GetState() != Weapon::State::kReturning) {
				// El ciclo salió de "regresando" por otra vía; nada que
				// mover ni que limpiar aquí (ya se habrá encargado quien
				// haya provocado el cambio de estado).
				g_active = false;
				return;
			}

			auto refr = a_handle.get();
			auto player = a_playerHandle.get();
			if (!refr || !player) {
				FinishReturn(a_handle);
				return;
			}

			const auto handPos = GetHandPosition(player.get());
			const auto currentPos = refr->GetPosition();

			if ((handPos - currentPos).Length() <= Constants::kReturnArrivalDistance) {
				FinishReturn(a_handle);
				return;
			}

			const auto nextPos = ComputeNextPosition(currentPos, handPos, Constants::kReturnSpeed, kTickDeltaSeconds);
			refr->SetPosition(nextPos);
			SetHavokPosition(*refr, nextPos);
			refr->Update3DPosition(true);
		}

		void RunLoop(RE::ObjectRefHandle a_handle, RE::ActorHandle a_playerHandle)
		{
			while (g_active.load()) {
				std::this_thread::sleep_for(kTickInterval);
				if (!g_active.load()) {
					return;
				}

				SKSE::GetTaskInterface()->AddTask([a_handle, a_playerHandle]() { Tick(a_handle, a_playerHandle); });
			}
		}

		void StartControlling(RE::Actor* a_player, RE::ObjectRefHandle a_handle)
		{
			auto refr = a_handle.get();
			if (!a_player || !refr) {
				return;
			}

			// Modo Havok "movido por código": deja de recibir fuerzas/
			// gravedad de la simulación pero conserva colisión; sin esto,
			// forzar la posición de un cuerpo simulado activamente produce
			// tirones/clipping (ver CLAUDE.md).
			auto* node3D = refr->Get3D();
			if (node3D) {
				node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true);
			}
			SetHavokPosition(*refr, refr->GetPosition());
			refr->Update3DPosition(true);
			logs::info("Return::StartControlling arrancando en ({:.1f},{:.1f},{:.1f})", refr->GetPosition().x, refr->GetPosition().y, refr->GetPosition().z);

			g_active = true;
			std::thread(RunLoop, a_handle, RE::ActorHandle(a_player)).detach();
		}

		// La réplica recién creada (ver Throw::SpawnWeaponReplicaAt) no
		// tiene el modelo 3D listo de inmediato — carga en segundo plano —
		// y hasta entonces SetMotionType/SetPosition no tienen ningún
		// efecto. Se espera a que Get3D() deje de ser nulo antes de empezar
		// a controlarla.
		void WaitFor3DThenStart(RE::Actor* a_player, RE::ObjectRefHandle a_handle, int a_attemptsLeft)
		{
			auto refr = a_handle.get();
			if (!a_player || !refr) {
				return;
			}

			if (refr->Get3D()) {
				StartControlling(a_player, a_handle);
				return;
			}

			if (a_attemptsLeft <= 0) {
				// El 3D nunca llegó a cargar; red de seguridad para no
				// dejar el ciclo bloqueado en "regresando" para siempre.
				logs::warn("WaitFor3DThenStart: agotados los reintentos, el 3D nunca llegó a cargar");
				refr->Disable();
				refr->SetDelete(true);
				if (Weapon::WeaponManager::GetSingleton()->GetState() == Weapon::State::kReturning) {
					Weapon::WeaponManager::GetSingleton()->OnReturnComplete();
				}
				return;
			}

			std::thread([a_player, a_handle, a_attemptsLeft]() {
				std::this_thread::sleep_for(kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_player, a_handle, a_attemptsLeft]() {
					WaitFor3DThenStart(a_player, a_handle, a_attemptsLeft - 1);
				});
			}).detach();
		}
	}

	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_handle)
	{
		WaitFor3DThenStart(a_player, a_handle, kMax3DWaitAttempts);
	}
}
