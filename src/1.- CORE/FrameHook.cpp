// Hook por fotograma -- ver FrameHook.h.

#include "1.- CORE/FrameHook.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <atomic>
#include <chrono>
#include <exception>

namespace FrameHook
{
	namespace
	{
		// Posición de Actor::Update(float) en la vtable de SE/AE (Actor.h).
		constexpr std::size_t kUpdateVtableIndex = 0xAD;

		std::atomic<bool> g_installed{ false };

		// Reloj de juego; solo lo escribe el hook (hilo principal).
		std::atomic<double> g_gameClock{ 0.0 };

		struct PlayerUpdateHook
		{
			// Primero el original (el juego y los hooks de otros mods); después el reloj, los temporizadores y los bucles.
			static void thunk(RE::PlayerCharacter* a_this, float a_delta)
			{
				func(a_this, a_delta);

				auto* ui = RE::UI::GetSingleton();
				if (!ui || ui->GameIsPaused()) {
					return;
				}

				const float delta = RE::GetSecondsSinceLastFrame();
				if (!(delta > 0.0f)) {
					return;
				}

				g_gameClock.store(g_gameClock.load() + delta);

				// Ninguna excepción debe cruzar al motor.
				try {
					Scheduler::RunFrame(delta);
					Physics::RunFrame(delta);
				} catch (const std::exception& e) {
					logs::error("FrameHook: excepción en los bucles por fotograma: {}", e.what());
				} catch (...) {
					logs::error("FrameHook: excepción desconocida en los bucles por fotograma.");
				}
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	bool Install()
	{
		if (REL::Module::IsVR()) {
			logs::warn("FrameHook: VR sin soporte, los bucles de la réplica siguen con hilos de {} ms.", Constants::kTickInterval.count());
			return false;
		}

		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_PlayerCharacter[0] };
		PlayerUpdateHook::func = vtbl.write_vfunc(kUpdateVtableIndex, PlayerUpdateHook::thunk);
		g_installed.store(true);

		logs::info("FrameHook: hook de PlayerCharacter::Update instalado, los bucles de la réplica van por fotograma.");
		return true;
	}

	bool IsInstalled() noexcept
	{
		return g_installed.load();
	}

	double Now() noexcept
	{
		if (g_installed.load()) {
			return g_gameClock.load();
		}

		return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	}
}
