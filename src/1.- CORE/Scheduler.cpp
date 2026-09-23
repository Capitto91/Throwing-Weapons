// Implementación del temporizador diferido centralizado.
// Ver Scheduler.h para el porqué de cada decisión.

#include "1.- CORE/Scheduler.h"

#include <thread>

namespace Scheduler
{
	CancelToken After(std::chrono::milliseconds a_delay, std::function<void()> a_callback)
	{
		auto active = std::make_shared<std::atomic<bool>>(true);

		logs::info("Scheduler::After: tarea programada, dispara en {}ms si no se cancela antes.", a_delay.count());

		std::thread([a_delay, callback = std::move(a_callback), active]() {
			std::this_thread::sleep_for(a_delay);

			// Nunca se reencola llamando a AddTask desde dentro de la
			// propia tarea que se ejecuta -- este hilo aparte es el único
			// que duerme y reencola, mismo criterio que
			// Physics::StartTickLoop (ver CLAUDE.md).
			SKSE::GetTaskInterface()->AddTask([callback, active]() {
				if (active->load()) {
					callback();
				} else {
					logs::info("Scheduler::After: tarea cancelada antes de dispararse, no se ejecuta.");
				}
			});
		}).detach();

		return active;
	}

	void Cancel(const CancelToken& a_token)
	{
		if (a_token) {
			a_token->store(false);
		}
	}
}
