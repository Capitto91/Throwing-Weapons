// Implementación del sistema de entrada.
// Traduce las acciones del jugador en órdenes para el controlador del arma.

#include "2.- INPUT/InputManager.h"

#include "1.- CORE/Constants.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "3.- WEAPON/WeaponManager.h"

#include <SimpleIni.h>

namespace Input
{
    namespace
    {
        RE::INPUT_DEVICE ParseDevice(std::string_view a_value)
        {
            if (a_value == "Mouse") {
                return RE::INPUT_DEVICE::kMouse;
            }
            if (a_value == "Gamepad") {
                return RE::INPUT_DEVICE::kGamepad;
            }
            return RE::INPUT_DEVICE::kKeyboard;
        }
    }

    InputManager* InputManager::GetSingleton()
    {
        static InputManager singleton;
        return &singleton;
    }

    void InputManager::Init()
    {
        LoadConfig();
        RE::BSInputDeviceManager::GetSingleton()->AddEventSink(this);

        logs::info(
            "InputManager listo (dispositivo: {}, código: {})",
            static_cast<std::uint32_t>(aimBinding.device),
            aimBinding.keyCode);
    }

    void InputManager::LoadConfig()
    {
        // Valores por defecto: tecla R del teclado (DIK_R = 0x13).
        aimBinding.device = RE::INPUT_DEVICE::kKeyboard;
        aimBinding.keyCode = 0x13;

        CSimpleIniA ini;
        ini.SetUnicode();

        if (ini.LoadFile(Constants::kInputConfigPath) < 0) {
            logs::warn("No se encontró {}, se usan los controles por defecto.", Constants::kInputConfigPath);
            return;
        }

        aimBinding.device = ParseDevice(ini.GetValue("Controls", "AimDevice", "Keyboard"));
        aimBinding.keyCode = static_cast<std::uint32_t>(
            ini.GetLongValue("Controls", "AimKeyCode", static_cast<long>(aimBinding.keyCode)));
    }

    bool InputManager::IsAimBinding(const RE::ButtonEvent* a_event) const
    {
        return a_event->GetDevice() == aimBinding.device && a_event->GetIDCode() == aimBinding.keyCode;
    }

    RE::BSEventNotifyControl InputManager::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
    {
        if (!a_event) {
            return RE::BSEventNotifyControl::kContinue;
        }

        // No procesar entrada mientras haya un menú abierto (inventario,
        // diálogo, etc.), igual que hace el propio juego.
        if (auto* ui = RE::UI::GetSingleton(); !ui || ui->GameIsPaused()) {
            return RE::BSEventNotifyControl::kContinue;
        }

        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* weaponManager = Weapon::WeaponManager::GetSingleton();

        // El botón participa en el ciclo si el arma arrojadiza está en la
        // mano derecha (para empezar a apuntar) o si el ciclo ya está en
        // marcha y el arma está fuera de la mano (para recuperarla). Con
        // el arma fuera, la mano queda vacía (ver WeaponManager::ThrowWeapon),
        // así que la comprobación de equipada por sí sola no basta.
        const bool participa = player &&
                                (weaponManager->GetState() != Weapon::State::kInHand ||
                                    ActorUtils::IsThrowableWeaponEquipped(player));

        if (!participa) {
            return RE::BSEventNotifyControl::kContinue;
        }

        for (auto* event = *a_event; event; event = event->next) {
            const auto* button = event->AsButtonEvent();
            if (!button || !IsAimBinding(button)) {
                continue;
            }

            if (button->IsDown()) {
                weaponManager->OnAimButtonDown();
            } else if (button->IsUp()) {
                weaponManager->OnAimButtonUp();
            }
        }

        return RE::BSEventNotifyControl::kContinue;
    }
}