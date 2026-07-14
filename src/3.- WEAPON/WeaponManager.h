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

		// Arma comprometida con el ciclo actual (nullptr si está "en
		// mano"). La usarán los módulos de detección de impacto a partir
		// de la Fase 4/5 para identificar si un golpe lo causó nuestra
		// réplica.
		[[nodiscard]] RE::TESBoundObject* GetActiveWeapon() const noexcept { return weaponState.GetActiveWeapon(); }

		// Fuerza la vuelta a "en mano" sin tocar el arma física, olvidando
		// cualquier arma activa. Se usa al cargar/empezar partida: tras
		// reiniciar el proceso no hay forma fiable de saber si el arma no
		// equipada es por nuestro ciclo o por decisión del jugador (nunca
		// la equipó, la vendió...), así que no se fuerza nada y se deja tal
		// cual está en el guardado.
		void ResetToInHand();

		// Si el ciclo está en marcha (apuntando o lanzada), recupera el
		// arma de inmediato (incluye destruir la réplica en vuelo si la
		// hay). Pensado para cuando se cierra una pantalla de carga
		// (puerta, viaje rápido...).
		void OnLoadingScreenClosed();

	private:
		WeaponManager() = default;
		~WeaponManager() = default;

		// Fija como arma activa la que hay en la mano derecha y pasa a
		// "apuntando".
		void BeginAiming();

		// Desequipa el arma activa (queda oculta y el jugador pasa a
		// combate desarmado), pasa a estado "lanzada" y arranca
		// Throw::LaunchWeapon para que la réplica visual vuele de verdad.
		void ThrowWeapon();

		// Recuperación instantánea: destruye la réplica en vuelo (si la
		// hay, ver Physics::DestroyReplica) y reequipa el arma de
		// inmediato, sin trayectoria de vuelta. 5.- RETURN sustituirá esta
		// transición por un regreso animado a partir de la Fase 6, dejando
		// esta función como red de seguridad (pantalla de carga a mitad del
		// trayecto, sin jugador del que partir, etc.), igual que documenta
		// CHANGELOG.md de la iteración anterior.
		void RecallWeapon();

		WeaponState weaponState;
	};
}
