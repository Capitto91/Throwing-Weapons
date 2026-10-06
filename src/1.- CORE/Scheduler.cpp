// Temporizador diferido -- ver Scheduler.h.

#include "1.- CORE/Scheduler.h"

#include "1.- CORE/FrameHook.h"

#include <mutex>
#include <thread>
#include <vector>

namespace Scheduler
{
	namespace
	{
		struct Timer
		{
			float                 remaining;
			std::function<void()> callback;
			CancelToken           active;
			bool                  fired{ false };
		};

		// Tareas recién creadas; RunFrame las incorpora al empezar el fotograma siguiente.
		std::mutex         g_pendingLock;
		std::vector<Timer> g_pendingTimers;

		// Tareas en cuenta atrás; solo las toca RunFrame (hilo del hook).
		std::vector<Timer> g_timers;

		// Encola a_callback en el hilo principal; no se ejecuta si se cancela antes.
		void Dispatch(std::function<void()> a_callback, CancelToken a_active)
		{
			SKSE::GetTaskInterface()->AddTask([callback = std::move(a_callback), active = std::move(a_active)]() {
				if (active->load()) {
					callback();
				}
			});
		}
	}

	CancelToken After(std::chrono::milliseconds a_delay, std::function<void()> a_callback)
	{
		auto active = std::make_shared<std::atomic<bool>>(true);

		if (FrameHook::IsInstalled()) {
			std::scoped_lock lock(g_pendingLock);
			g_pendingTimers.push_back(Timer{ std::chrono::duration<float>(a_delay).count(), std::move(a_callback), active });
			return active;
		}

		// Sin FrameHook: hilo que duerme a_delay real; este hilo aparte es quien reencola, nunca una tarea a sí misma.
		std::thread([a_delay, callback = std::move(a_callback), active]() mutable {
			std::this_thread::sleep_for(a_delay);
			Dispatch(std::move(callback), active);
		}).detach();

		return active;
	}

	void Cancel(CancelToken& a_token)
	{
		if (a_token) {
			a_token->store(false);
			a_token.reset();
		}
	}

	void RunFrame(float a_deltaSeconds)
	{
		{
			std::scoped_lock lock(g_pendingLock);
			for (auto& timer : g_pendingTimers) {
				g_timers.push_back(std::move(timer));
			}
			g_pendingTimers.clear();
		}

		for (auto& timer : g_timers) {
			if (!timer.active->load()) {
				continue;
			}

			timer.remaining -= a_deltaSeconds;
			if (timer.remaining <= 0.0f) {
				Dispatch(std::move(timer.callback), timer.active);
				timer.fired = true;
			}
		}

		std::erase_if(g_timers, [](const Timer& a_timer) { return a_timer.fired || !a_timer.active->load(); });
	}

	std::size_t GetPendingTimerCount()
	{
		std::scoped_lock lock(g_pendingLock);
		return g_timers.size() + g_pendingTimers.size();
	}
}
