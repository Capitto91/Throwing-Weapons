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

		// Detiene el hilo de fondo, destruye el proyectil controlado a mano y
		// notifica a WeaponManager para que reequipe el arma real. Comprueba
		// el estado antes de notificar por si el ciclo ya terminó por otra
		// vía (p. ej. recuperación instantánea al cerrar una pantalla de
		// carga a mitad del regreso), para no reequipar ni resetear el
		// estado dos veces.
		void FinishReturn(RE::ProjectileHandle a_handle)
		{
			g_active = false;

			if (auto projectile = a_handle.get()) {
				projectile->Kill();
			}

			if (Weapon::WeaponManager::GetSingleton()->GetState() == Weapon::State::kReturning) {
				Weapon::WeaponManager::GetSingleton()->OnReturnComplete();
			}
		}

		void Tick(RE::ProjectileHandle a_handle, RE::ActorHandle a_playerHandle)
		{
			if (Weapon::WeaponManager::GetSingleton()->GetState() != Weapon::State::kReturning) {
				// El ciclo salió de "regresando" por otra vía; nada que
				// mover ni que limpiar aquí (ya se habrá encargado quien
				// haya provocado el cambio de estado).
				g_active = false;
				return;
			}

			auto projectile = a_handle.get();
			auto player = a_playerHandle.get();
			if (!projectile || !player) {
				FinishReturn(a_handle);
				return;
			}

			const auto handPos = GetHandPosition(player.get());
			const auto currentPos = projectile->GetPosition();

			if ((handPos - currentPos).Length() <= Constants::kReturnArrivalDistance) {
				FinishReturn(a_handle);
				return;
			}

			projectile->SetPosition(ComputeNextPosition(currentPos, handPos, Constants::kReturnSpeed, kTickDeltaSeconds));
		}

		void RunLoop(RE::ProjectileHandle a_handle, RE::ActorHandle a_playerHandle)
		{
			while (g_active.load()) {
				std::this_thread::sleep_for(kTickInterval);
				if (!g_active.load()) {
					return;
				}

				SKSE::GetTaskInterface()->AddTask([a_handle, a_playerHandle]() { Tick(a_handle, a_playerHandle); });
			}
		}
	}

	void BeginReturn(RE::Actor* a_player, RE::ProjectileHandle a_handle)
	{
		auto projectile = a_handle.get();
		if (!a_player || !projectile) {
			return;
		}

		// Modo Havok "movido por código": deja de recibir fuerzas/gravedad
		// de la simulación pero conserva colisión; sin esto, forzar la
		// posición de un cuerpo simulado activamente produce tirones/
		// clipping (ver CLAUDE.md).
		projectile->SetMotionType(RE::hkpMotion::MotionType::kKeyframed);

		g_active = true;
		std::thread(RunLoop, a_handle, RE::ActorHandle(a_player)).detach();
	}
}
