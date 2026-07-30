// Gestiona la entrada del jugador relacionada con el arma.
// Controla pulsación, apuntado y liberación del botón para lanzar o recuperar
// el arma.

#pragma once

namespace Input
{
	// Dispositivo y código de botón configurados por el usuario para la
	// acción de apuntar / soltar (que dispara o recupera el arma).
	struct AimBinding
	{
		RE::INPUT_DEVICE device{ RE::INPUT_DEVICE::kKeyboard };
		std::uint32_t    keyCode{ 0 };
	};

	class InputManager final : public RE::BSTEventSink<RE::InputEvent*>
	{
	public:
		static InputManager* GetSingleton();

		InputManager(const InputManager&) = delete;
		InputManager(InputManager&&) = delete;
		InputManager& operator=(const InputManager&) = delete;
		InputManager& operator=(InputManager&&) = delete;

		// Carga la configuración de controles y se registra para recibir
		// eventos de entrada. Debe llamarse una única vez, tras kInputLoaded.
		void Init();

	protected:
		RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>* a_eventSource) override;

	private:
		InputManager() = default;
		~InputManager() override = default;

		void LoadConfig();
		bool IsAimBinding(const RE::ButtonEvent* a_event) const;

		AimBinding aimBinding{};
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
