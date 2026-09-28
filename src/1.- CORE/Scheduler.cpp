// Temporizador diferido -- ver Scheduler.h.

#include "1.- CORE/Scheduler.h"

#include <thread>

namespace Scheduler
{
	CancelToken After(std::chrono::milliseconds a_delay, std::function<void()> a_callback)
	{
		auto active = std::make_shared<std::atomic<bool>>(true);


		std::thread([a_delay, callback = std::move(a_callback), active]() {
			std::this_thread::sleep_for(a_delay);

			// Este hilo aparte es quien reencola; nunca una tarea a sí misma.
			SKSE::GetTaskInterface()->AddTask([callback, active]() {
				if (active->load()) {
					callback();
				} else {
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
