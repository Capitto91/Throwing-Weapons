// Implementación del sistema de entrada.
// Traduce las acciones del jugador en órdenes para el controlador del arma.

#include "2.- INPUT/InputManager.h"

#include "1.- CORE/Settings.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "3.- WEAPON/WeaponManager.h"

namespace Input
{
	InputManager* InputManager::GetSingleton()
	{
		static InputManager singleton;
		return &singleton;
	}

	void InputManager::Init()
	{
		RE::BSInputDeviceManager::GetSingleton()->AddEventSink(this);

		const auto binding = Settings::GetActionBinding();
		logs::info("InputManager listo (dispositivo: {}, código: {})", Settings::DeviceToString(binding.device), binding.keyCode);
	}

	bool InputManager::IsActionBinding(const RE::ButtonEvent* a_event)
	{
		const auto binding = Settings::GetActionBinding();
		return a_event->GetDevice() == binding.device && a_event->GetIDCode() == binding.keyCode;
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
		// mano derecha (para lanzarla) o si el ciclo ya está en
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
			if (!button || !IsActionBinding(button)) {
				continue;
			}

			if (button->IsDown()) {
				weaponManager->OnActionButtonDown();
			} else if (button->IsUp()) {
				weaponManager->OnActionButtonUp();
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	void SetMovementLocked(bool a_locked)
	{
		RE::ControlMap::GetSingleton()->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMovement, !a_locked, true);
	}
}
