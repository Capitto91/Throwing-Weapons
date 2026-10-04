// Vigilante de grafo asentado -- ver GraphSettleWatcher.h.

#include "10.- EVENTS/GraphSettleWatcher.h"

#include <atomic>
#include <mutex>
#include <vector>

namespace Events::GraphSettleWatcher
{
	namespace
	{
		// Escritos por el sink desde los hilos de animación y leídos en el hilo principal.
		std::atomic<bool> drawPending{ false };
		std::atomic<bool> attackStopPending{ false };

		// Fuentes de evento ya enganchadas (solo para no duplicar; no se desreferencian).
		std::mutex               sourcesMutex;
		std::vector<const void*> sources;

		class Sink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				if (!a_event) {
					return RE::BSEventNotifyControl::kContinue;
				}

				const std::string_view tag = a_event->tag.c_str();
				if (tag == "BeginWeaponDraw") {
					drawPending = true;
				} else if (tag == "WeapEquip_Out" || tag == "WeapEquip_OutMoving") {
					drawPending = false;
				} else if (tag == "attackStop") {
					attackStopPending = false;
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		Sink sink;
	}

	void Track(RE::Actor& a_player)
	{
		RE::BSTSmartPointer<RE::BSAnimationGraphManager> manager;
		if (!a_player.GetAnimationGraphManager(manager) || !manager) {
			return;
		}

		// Los grafos cambian si se recarga el 3D; se enganchan los que falten.
		for (auto& graph : manager->graphs) {
			auto* source = graph ? graph->GetEventSource<RE::BSAnimationGraphEvent>() : nullptr;
			if (!source) {
				continue;
			}
			{
				std::lock_guard lock(sourcesMutex);
				if (std::ranges::find(sources, source) != sources.end()) {
					continue;
				}
				sources.push_back(source);
			}
			source->AddEventSink(&sink);
		}
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
