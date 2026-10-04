// Entrada del jugador -- ver InputManager.h.

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

	void InputManager::HandleActionButton(bool a_down)
	{
		// Nada con el juego en pausa (menús abiertos).
		if (auto* ui = RE::UI::GetSingleton(); !ui || ui->GameIsPaused()) {
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weaponManager = Weapon::WeaponManager::GetSingleton();

		// Participa con el martillo en la mano (lanzar) o con el ciclo en marcha (recuperar).
		const bool participa = player &&
		                       (weaponManager->GetState() != Weapon::State::kInHand ||
								   ActorUtils::IsThrowableWeaponEquipped(player));

		if (!participa) {
			return;
		}

		if (a_down) {
			weaponManager->OnActionButtonDown();
		} else {
			weaponManager->OnActionButtonUp();
		}
	}

	RE::BSEventNotifyControl InputManager::ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}

		// Llega por un hilo del motor: aquí solo se reconoce la tecla; HandleActionButton, en el hilo principal.
		for (auto* event = *a_event; event; event = event->next) {
			const auto* button = event->AsButtonEvent();
			if (!button || !IsActionBinding(button)) {
				continue;
			}

			if (button->IsDown()) {
				SKSE::GetTaskInterface()->AddTask([] { HandleActionButton(true); });
			} else if (button->IsUp()) {
				SKSE::GetTaskInterface()->AddTask([] { HandleActionButton(false); });
			}
		}

		return RE::BSEventNotifyControl::kContinue;
	}

	void SetMovementLocked(bool a_locked)
	{
		RE::ControlMap::GetSingleton()->ToggleControls(RE::UserEvents::USER_EVENT_FLAG::kMovement, !a_locked, true);
	}

	void SetCameraSwitchLocked(bool a_locked)
	{
		using UEFlag = RE::UserEvents::USER_EVENT_FLAG;

		// Controles que bloqueó esta función (los que ya estaban bloqueados por otros no se tocan).
		static bool povLockedHere = false;
		static bool wheelLockedHere = false;

		auto* controlMap = RE::ControlMap::GetSingleton();
		if (!controlMap) {
			return;
		}

		if (a_locked) {
			if (!povLockedHere && controlMap->IsPOVSwitchControlsEnabled()) {
				controlMap->ToggleControls(UEFlag::kPOVSwitch, false, true);
				povLockedHere = true;
			}
			if (!wheelLockedHere && controlMap->IsWheelZoomControlsEnabled()) {
				controlMap->ToggleControls(UEFlag::kWheelZoom, false, true);
				wheelLockedHere = true;
			}
			return;
		}

		if (povLockedHere) {
			controlMap->ToggleControls(UEFlag::kPOVSwitch, true, true);
			povLockedHere = false;
		}
		if (wheelLockedHere) {
			controlMap->ToggleControls(UEFlag::kWheelZoom, true, true);
			wheelLockedHere = false;
		}
	}
}
