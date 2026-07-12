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

        // Arma comprometida con el ciclo actual: se fija al empezar a
        // apuntar (para no depender de lo que haya en la mano en el
        // momento de soltar el botón) y se usa para reequiparla tal cual al
        // recuperarla.
        [[nodiscard]] RE::TESBoundObject* GetActiveWeapon() const noexcept { return activeWeapon; }
        void                              SetActiveWeapon(RE::TESBoundObject* a_weapon) noexcept { activeWeapon = a_weapon; }

    private:
        State               state{ State::kInHand };
        RE::TESBoundObject* activeWeapon{ nullptr };
    };
}