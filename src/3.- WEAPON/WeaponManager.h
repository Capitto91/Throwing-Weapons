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
		// mano"). Usado por Events::ProjectileHitWatcher para identificar
		// si un TESHitEvent lo causó nuestro proyectil.
		[[nodiscard]] RE::TESBoundObject* GetActiveWeapon() const noexcept { return weaponState.GetActiveWeapon(); }

		// Llamados desde Throw::TrackProjectile (4.- THROW) al vigilar el
		// proyectil en vuelo, o desde Events::ProjectileHitWatcher si el
		// impacto fue contra un actor. Todos ignoran la notificación si el
		// arma ya no está en "lanzada" (p. ej. el jugador ya pulsó
		// recuperar, o una pantalla de carga resincronizó el estado
		// primero), para no pisar una transición más reciente.
		void OnProjectileImpact();
		void OnProjectileMaxRangeReached();

		// Igual que OnProjectileMaxRangeReached (recuperación automática):
		// el agua no es una superficie donde el arma pueda quedar clavada,
		// y Mecanica del arma.txt no cubre este caso. Comprobado en el
		// juego que ImpactResult nunca cambia al caer al agua (se queda
		// flotando/hundiéndose sin "impactar"), así que se trata igual que
		// no impactar contra nada.
		void OnProjectileEnteredWater();

		// Fuerza la vuelta a "en mano" sin tocar el arma física, olvidando
		// cualquier arma activa. Se usa al cargar/empezar partida: tras
		// reiniciar el proceso no hay forma fiable de saber si el arma no
		// equipada es por nuestro ciclo o por decisión del jugador (nunca
		// la equipó, la vendió...), así que no se fuerza nada y se deja tal
		// cual está en el guardado.
		void ResetToInHand();

		// Si el ciclo está en marcha (apuntando, lanzada, clavada o
		// regresando), recupera el arma de inmediato. A diferencia de
		// ResetToInHand(), aquí sí sabemos con certeza qué arma es (la
		// recordamos al empezar a apuntar, en la misma sesión), así que es
		// seguro reequiparla. Pensado para cuando se cierra una pantalla de
		// carga (puerta, viaje rápido...): hoy no hay ningún proyectil real
		// que se pueda perder, así que no tiene sentido obligar al jugador
		// a recuperarla a mano.
		void OnLoadingScreenClosed();

	private:
		WeaponManager() = default;
		~WeaponManager() = default;

		// Fija como arma activa la que hay en la mano derecha y pasa a
		// "apuntando".
		void BeginAiming();

		// Desequipa el arma activa (queda oculta y el jugador pasa a
		// combate desarmado) y pasa a estado "lanzada". La réplica en forma
		// de proyectil la crea 4.- THROW; aquí solo se gestiona el arma
		// física del jugador.
		void ThrowWeapon();

		// Reequipa el arma activa y vuelve a "en mano". La recuperación es
		// instantánea por ahora: la trayectoria de regreso es
		// responsabilidad de 5.- RETURN, que sustituirá esta transición
		// directa una vez exista.
		void RecallWeapon();

		WeaponState weaponState;
	};
}