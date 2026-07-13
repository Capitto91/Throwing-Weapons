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
		// primero), para no pisar una transición más reciente. a_target es
		// el actor contra el que impactó, si se conoce (nullptr si fue
		// contra una superficie) — se guarda para poder localizar y
		// desenganchar el modelo clavado al recuperar (ver
		// Throw::DetachEmbeddedWeapon).
		void OnProjectileImpact(RE::TESObjectREFR* a_target = nullptr);

		// Recuperación automática (punto 5 de Mecanica del arma.txt): el
		// jugador no ha pulsado el botón, pero el arma vuelve igual que si
		// lo hubiera hecho, con el mismo regreso animado y curvo de
		// 5.- RETURN (BeginReturn()) — no un recall instantáneo.
		void OnProjectileMaxRangeReached();

		// Igual que OnProjectileMaxRangeReached (regreso automático y
		// animado, no instantáneo): el agua no es una superficie donde el
		// arma pueda quedar clavada, y Mecanica del arma.txt no cubre este
		// caso. Comprobado en el juego que ImpactResult nunca cambia al
		// caer al agua (se queda flotando/hundiéndose sin "impactar"), así
		// que se trata igual que no impactar contra nada.
		void OnProjectileEnteredWater();

		// Llamado por Return::BeginReturn (5.- RETURN), en el hilo
		// principal, cuando el proyectil controlado a mano ha llegado a la
		// mano del jugador. El proyectil ya ha sido destruido por el
		// propio módulo Return antes de llamar aquí; esto solo reequipa el
		// arma real y vuelve a "en mano".
		void OnReturnComplete();

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

		// Recuperación instantánea: destruye/desengancha el proyectil (según
		// esté en vuelo, clavado en superficie o clavado en un actor) y
		// reequipa el arma de inmediato, sin trayectoria. Se usa cuando no
		// hay forma de iniciar un regreso de verdad (sin jugador, sin
		// proyectil ni actor del que partir) y para abortar un regreso ya
		// en marcha (p. ej. al cerrarse una pantalla de carga a mitad del
		// trayecto).
		void RecallWeapon();

		// Empieza el regreso de verdad (5.- RETURN). En los tres puntos de
		// partida posibles (en vuelo, clavada en superficie, clavada en un
		// actor) se captura la posición actual, se destruye/desengancha lo
		// que hubiera hasta ahora, y se lanza una réplica visual nueva ahí
		// mismo (ver Throw::SpawnWeaponReplicaAt) para que Return la
		// controle — nunca se reutiliza el Projectile nativo de la ida
		// (comprobado en el juego: un Projectile todavía activo en Havok
		// compite con el control manual tick a tick; ver CHANGELOG.md
		// v0.3.x). Si no hay forma de arrancarlo (sin jugador o sin punto
		// de partida), cae a RecallWeapon() como red de seguridad.
		void BeginReturn();

		// Caso "clavado en un actor": intenta desenganchar el nodo 3D
		// (Throw::DetachEmbeddedWeapon) y, si lo consigue, lanza la réplica
		// y arranca el regreso. El nodo puede tardar en aparecer tras el
		// impacto (ver Constants::kEmbeddedWeaponNodeName); si todavía no
		// está, reintenta en segundo plano en vez de rendirse a la primera.
		// El jugador nunca debe ver el arma reaparecer sin animación: si
		// a_attemptsLeft llega a 0 sin encontrar el nodo, arranca el
		// regreso igualmente desde la posición actual del actor (menos
		// preciso que el punto exacto del clavado, pero siempre animado)
		// en vez de caer a RecallWeapon().
		void TryDetachAndBeginReturn(RE::ObjectRefHandle a_targetHandle, int a_attemptsLeft);

		// Llamado por TryDetachAndBeginReturn cuando se agotan sus
		// reintentos sin encontrar el nodo: el regreso ya ha arrancado con
		// la posición aproximada del actor a esas alturas (esto no la
		// afecta), pero si el nodo "Scene Root" aparece más tarde se
		// quedaría enganchado al actor para siempre como un arma fantasma.
		// Sigue vigilando en segundo plano, sin bloquear nada, hasta
		// Constants::kLateEmbeddedWeaponWatchAttempts intentos extra: si lo
		// encuentra, lo desengancha (limpieza, ignora la posición) y deja
		// constancia en el log de cuánto tardó de verdad — dato para poder
		// ajustar los reintentos normales en vez de a ciegas.
		void WatchForLateEmbeddedWeapon(RE::ObjectRefHandle a_targetHandle, int a_attemptsLeft);

		// Lanza la réplica visual en a_position (ver
		// Throw::SpawnWeaponReplicaAt) y arranca Return::BeginReturn sobre
		// ella. Punto de partida común a los tres casos de BeginReturn().
		void SpawnReplicaAndBeginReturn(RE::Actor* a_player, const RE::NiPoint3& a_position);

		// Reequipa el arma activa y vuelve a "en mano", sin tocar el
		// proyectil (ya limpiado por el llamante). Código compartido entre
		// RecallWeapon() y OnReturnComplete().
		void ReequipAndReset();

		WeaponState weaponState;
	};
}