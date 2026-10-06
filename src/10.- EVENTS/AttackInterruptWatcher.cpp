// Vigilante del corte de ataque -- ver AttackInterruptWatcher.h.

#include "10.- EVENTS/AttackInterruptWatcher.h"

#include "1.- CORE/Constants.h"

#include <chrono>
#include <mutex>

namespace Events::AttackInterruptWatcher
{
	namespace
	{
		// Estado compartido con los hilos de animación: mutex.
		std::mutex                            mutex;
		bool                                  armed{ false };
		std::chrono::steady_clock::time_point armedAt;
		int                                   attackStopCount{ 0 };
		std::function<void()>                 onReady;

		class Sink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}

				std::function<void()> callback;
				{
					std::lock_guard lock(mutex);
					if (!armed) {
						return RE::BSEventNotifyControl::kContinue;
					}

					const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - armedAt);
					if (elapsed > Constants::kAttackInterruptWatchWindow) {
						armed = false;
						onReady = nullptr;
						return RE::BSEventNotifyControl::kContinue;
					}

					if (a_event->tag == Constants::kAttackStopAnimationEvent &&
						++attackStopCount == Constants::kAttackInterruptReadyEventOrdinal) {
						armed = false;
						callback = std::move(onReady);
						onReady = nullptr;
					}
				}

				// Fuera del lock, AddTask desde el hilo de animación.
				if (callback) {
					SKSE::GetTaskInterface()->AddTask(std::move(callback));
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		Sink sink;
	}

	void Arm(RE::Actor& a_actor, std::function<void()> a_onReady)
	{
		// Solo el primer grafo, a propósito: se cuentan los attackStop, y en primera persona los dos grafos los
		// emiten (con ActorUtils::AddEventSinkToAllGraphs se contarían dos veces).
		a_actor.AddAnimationGraphEventSink(&sink);

		std::lock_guard lock(mutex);
		armed = true;
		armedAt = std::chrono::steady_clock::now();
		attackStopCount = 0;
		onReady = std::move(a_onReady);
	}

	void Disarm()
	{
		std::lock_guard lock(mutex);
		armed = false;
		onReady = nullptr;
	}
}
