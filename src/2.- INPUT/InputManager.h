// Lee la tecla de lanzar/recuperar (Settings) y avisa a WeaponManager al pulsar y soltar.

#pragma once

namespace Input
{
	class InputManager final : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		static InputManager* GetSingleton();

		InputManager(const InputManager&) = delete;
		InputManager(InputManager&&) = delete;
		InputManager& operator=(const InputManager&) = delete;
		InputManager& operator=(InputManager&&) = delete;

		// Registra el sink de entrada. Lo llama EventManager en kInputLoaded.
		void Init();

	protected:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

	private:
		InputManager() = default;
		~InputManager() override = default;

		static bool IsActionBinding(const RE::ButtonEvent* a_event);

		// Pulsar (a_down) o soltar la tecla: avisa a WeaponManager si procede.
		// Lo encola ProcessEvent con AddTask, así que corre en el hilo principal.
		static void HandleActionButton(bool a_down);
	};

	// Bloquea o desbloquea el movimiento del jugador (RE::ControlMap).
	// Lo usa WeaponManager en los gestos para que no escalen a power attack direccional.
	void SetMovementLocked(bool a_locked);
}
