// Controlador principal del arma original.
// Gestiona ocultar, desactivar, activar y restaurar el arma física del jugador.

#pragma once

#include "3.- WEAPON/WeaponState.h"

namespace Weapon
{
    class WeaponManager
    {
    public:
        static WeaponManager* GetSingleton();

        WeaponManager(const WeaponManager&) = delete;
        WeaponManager(WeaponManager&&) = delete;
        WeaponManager& operator=(const WeaponManager&) = delete;
        WeaponManager& operator=(WeaponManager&&) = delete;

        // Llamados desde Input::InputManager cuando se pulsa/suelta el botón
        // de apuntar. Deciden, según el estado actual, si hay que empezar a
        // apuntar, lanzar el arma o recuperarla.
        void OnAimButtonDown();
        void OnAimButtonUp();

        [[nodiscard]] State GetState() const noexcept { return weaponState.GetState(); }

    private:
        WeaponManager() = default;
        ~WeaponManager() = default;

        // Desequipa el arma original (queda oculta y el jugador pasa a
        // combate desarmado) y pasa a estado "lanzada". La réplica en forma
        // de proyectil la crea 4.- THROW; aquí solo se gestiona el arma
        // física del jugador.
        void ThrowWeapon();

        // Reequipa el arma original y vuelve a "en mano". La recuperación es
        // instantánea por ahora: la trayectoria de regreso es
        // responsabilidad de 5.- RETURN, que sustituirá esta transición
        // directa una vez exista.
        void RecallWeapon();

        WeaponState weaponState;
    };
}