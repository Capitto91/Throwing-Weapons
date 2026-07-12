// Define el estado actual del arma durante su ciclo de uso.
// Guarda referencias, estados internos y datos necesarios para saber dónde está
// el arma y qué está ocurriendo con ella.

#pragma once

namespace Weapon
{
    // Ciclo de vida del arma arrojadiza única.
    enum class State
    {
        kInHand,    // Equipada normalmente en la mano derecha.
        kAiming,    // Botón pulsado, apuntando antes de soltar.
        kThrown,    // Lanzada: en vuelo, fuera de la mano.
        kStuck,     // Clavada en un enemigo o superficie.
        kReturning  // Volviendo hacia el jugador.
    };

    class WeaponState
    {
    public:
        [[nodiscard]] State GetState() const noexcept { return state; }
        void                SetState(State a_state);

        // Arma original que se ocultó al lanzar, para poder reequiparla tal
        // cual al recuperarla.
        [[nodiscard]] RE::TESBoundObject* GetThrownWeapon() const noexcept { return thrownWeapon; }
        void                              SetThrownWeapon(RE::TESBoundObject* a_weapon) noexcept { thrownWeapon = a_weapon; }

    private:
        State               state{ State::kInHand };
        RE::TESBoundObject* thrownWeapon{ nullptr };
    };
}