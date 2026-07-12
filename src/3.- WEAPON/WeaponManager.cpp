// Implementación del ciclo de vida del arma.
// Coordina la transición entre arma en mano, arma lanzada y arma recuperada.

#include "3.- WEAPON/WeaponManager.h"

#include "4.- THROW/ThrowManager.h"

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

    void WeaponManager::OnLoadingScreenClosed()
    {
        switch (weaponState.GetState()) {
        case State::kThrown:
        case State::kStuck:
        case State::kReturning:
            RecallWeapon();
            break;
        case State::kAiming:
            // El arma sigue en la mano (el desequipar solo pasa al soltar);
            // solo reordenamos el estado por si la pulsación de soltar se
            // perdió durante la carga.
            weaponState.SetState(State::kInHand);
            break;
        default:
            break;
        }
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
            // El proyectil réplica se crea antes de desequipar, mientras el
            // arma todavía está en la mano (para tomar su posición justo
            // ahí, ver Throw::LaunchWeapon), y "al mismo tiempo" (Mecanica
            // del arma.txt, punto 2) el arma original se oculta.
            Throw::LaunchWeapon(player, weapon->As<RE::TESObjectWEAP>());

            // Sin cola y aplicación inmediata: un desequipar encolado podía
            // perderse en silencio si se dispara desde un evento de carga.
            RE::ActorEquipManager::GetSingleton()->UnequipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
        }

        weaponState.SetState(State::kThrown);
    }

    void WeaponManager::RecallWeapon()
    {
        auto* player = RE::PlayerCharacter::GetSingleton();
        auto* weapon = weaponState.GetActiveWeapon();

        if (player && weapon) {
            // Se difiere al siguiente tick (tarea de SKSE) en vez de
            // llamarlo aquí mismo: invocado justo al cerrarse una pantalla
            // de carga, el juego aceptaba la orden (sonaba el sonido de
            // equipar) pero nunca llegaba a equipar el arma de verdad.
            SKSE::GetTaskInterface()->AddTask([player, weapon]() {
                RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
            });
        }

        weaponState.SetActiveWeapon(nullptr);
        weaponState.SetState(State::kInHand);
    }
}