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
            BeginAiming();
            break;
        case State::kAiming:
            // Solo se puede recibir una pulsación nueva estando ya
            // "apuntando" si nos perdimos el botón de soltar anterior (p.ej.
            // una pantalla de carga a mitad de la pulsación): no hay nada
            // que deshacer todavía, así que reiniciamos el ciclo con la
            // pulsación actual en vez de quedarnos atascados.
            weaponState.SetState(State::kInHand);
            BeginAiming();
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

    void WeaponManager::ResetToInHand()
    {
        weaponState.SetActiveWeapon(nullptr);
        weaponState.SetState(State::kInHand);
    }

    void WeaponManager::BeginAiming()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* weapon = player ? player->GetEquippedObject(false) : nullptr;
        auto* boundWeapon = weapon ? weapon->As<RE::TESBoundObject>() : nullptr;

        if (!boundWeapon) {
            return;
        }

        weaponState.SetActiveWeapon(boundWeapon);
        weaponState.SetState(State::kAiming);
    }

    void WeaponManager::ThrowWeapon()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* weapon = weaponState.GetActiveWeapon();

        if (player && weapon) {
            RE::ActorEquipManager::GetSingleton()->UnequipObject(player, weapon);
        }

        weaponState.SetState(State::kThrown);
    }

    void WeaponManager::RecallWeapon()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* weapon = weaponState.GetActiveWeapon();

        if (player && weapon) {
            RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon);
        }

        weaponState.SetActiveWeapon(nullptr);
        weaponState.SetState(State::kInHand);
    }
}