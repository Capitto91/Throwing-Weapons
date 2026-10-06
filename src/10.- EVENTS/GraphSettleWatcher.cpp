// Vigilante de grafo asentado -- ver GraphSettleWatcher.h.

#include "10.- EVENTS/GraphSettleWatcher.h"

#include "1.- CORE/Constants.h"
#include "11.- SKYRIM/ActorUtils.h"

#include <atomic>

namespace Events::GraphSettleWatcher
{
	namespace
	{
		// Escritos por el sink desde los hilos de animación y leídos en el hilo principal.
		std::atomic<bool> drawPending{ false };
		std::atomic<bool> attackStopPending{ false };

		class Sink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}

				const std::string_view tag = a_event->tag.c_str();
				if (tag == Constants::kWeaponDrawStartEvent) {
					drawPending = true;
				} else if (tag == Constants::kWeaponDrawEndEvent || tag == Constants::kWeaponDrawEndMovingEvent) {
					drawPending = false;
				} else if (tag == Constants::kAttackStopAnimationEvent) {
					attackStopPending = false;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		Sink sink;
	}

	void Track(RE::Actor& a_player)
	{
		// Los grafos cambian si se recarga el 3D; los ya enganchados se ignoran.
		(void)ActorUtils::AddEventSinkToAllGraphs(a_player, &sink);
	}

	void NoteDrawExpected()
	{
		drawPending = true;
	}

	void NoteAttackStopSent(bool a_pending)
	{
		attackStopPending = a_pending;
	}

	bool IsSettled()
	{
		return !drawPending && !attackStopPending;
	}

	void Reset()
	{
		drawPending = false;
		attackStopPending = false;
	}
}
