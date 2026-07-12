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

        // Puntos de entrada hacia el resto del sistema. Por ahora sólo
        // registran el evento en el log; 4.- THROW / 5.- RETURN se
        // engancharán aquí para lanzar y recuperar el arma.
        void OnAimStart() const;
        void OnAimRelease() const;

        AimBinding aimBinding{};
    };
}