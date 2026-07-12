// Implementación del ciclo de vida del arma.
// Coordina la transición entre arma en mano, arma lanzada y arma recuperada.

#include "3.- WEAPON/WeaponManager.h"

namespace Weapon
{
    WeaponManager* WeaponManager::GetSingleton()
    {
        static WeaponManager singleton;
        return &singleton;
    }

    void WeaponManager::OnAimButtonDown()
    {
        switch (weaponState.GetState()) {
        case State::kInHand:
            weaponState.SetState(State::kAiming);
            break;
        case State::kThrown:
        case State::kStuck:
            RecallWeapon();
            break;
        default:
            break;
        }
    }

    void WeaponManager::OnAimButtonUp()
    {
        if (weaponState.GetState() == State::kAiming) {
            ThrowWeapon();
        }
    }

    void WeaponManager::ThrowWeapon()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* weapon = player ? player->GetEquippedObject(false) : nullptr;
        auto* boundWeapon = weapon ? weapon->As<RE::TESBoundObject>() : nullptr;

        if (!player || !boundWeapon) {
            return;
        }

        RE::ActorEquipManager::GetSingleton()->UnequipObject(player, boundWeapon);
        weaponState.SetThrownWeapon(boundWeapon);
        weaponState.SetState(State::kThrown);
    }

    void WeaponManager::RecallWeapon()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* weapon = weaponState.GetThrownWeapon();

        if (player && weapon) {
            RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon);
        }

        weaponState.SetThrownWeapon(nullptr);
        weaponState.SetState(State::kInHand);
    }
}