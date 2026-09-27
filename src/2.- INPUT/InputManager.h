// Gestiona la entrada del jugador relacionada con el arma.
// Controla pulsación, apuntado y liberación del botón para lanzar o recuperar
// el arma.

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

		// Se registra para recibir eventos de entrada. Debe llamarse una
		// única vez, tras kInputLoaded. La tecla no se guarda aquí: se
		// consulta en Settings en cada evento, así que un cambio desde el
		// menú del juego se aplica al instante.
		void Init();

	protected:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

	private:
		InputManager() = default;
		~InputManager() override = default;

		static bool IsActionBinding(const RE::ButtonEvent* a_event);
	};

	// Bloquea/desbloquea el movimiento del jugador (RE::ControlMap, no una
	// graph variable propia) -- usado por WeaponManager durante
	// State::kThrowing para evitar el power attack direccional vanilla
	// (moverse mientras se ataca escala automáticamente a
	// 1HM_AttackPowerFwd/Bwd/Left/Right, un clip que el submod de OAR de
	// Lanzar no sustituye, así que se ve y se comporta como un ataque real
	// en vez de Throw.hkx -- comprobado en el juego con el Animation Event
	// Log de OAR, ver _reference/PLAN-OAR.md). a_storeState=true en la
	// llamada real (ver InputManager.cpp) para que el bloqueo/desbloqueo
	// componga bien si algún otro sistema también togglea el movimiento a
	// la vez, en vez de pisarse.
	void SetMovementLocked(bool a_locked);
}
