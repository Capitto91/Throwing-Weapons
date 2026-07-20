// Controlador principal del arma original.
// Gestiona ocultar, desactivar, activar y restaurar el arma física del jugador.

#pragma once

#include "3.- WEAPON/WeaponState.h"

namespace Weapon
{
	class WeaponManager
	{
	public:
		// Datos mínimos para reconstruir el ciclo si la partida se guardó a
		// medias (p. ej. a mitad de un lanzamiento) — ver Events::Init,
		// registro del cosave. FormID en vez de handles: los handles no
		// sobreviven a un guardado/carga, los FormID sí (remapeados con
		// SerializationInterface::ResolveFormID).
		struct SaveCycleData
		{
			bool       cycleActive{ false };
			RE::FormID weaponFormID{ 0 };
			RE::FormID replicaFormID{ 0 };
			RE::FormID stuckActorFormID{ 0 };
		};

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

		// Para el callback de guardado del cosave: qué persistir del ciclo
		// actual en este instante.
		[[nodiscard]] SaveCycleData CaptureSaveData() const;

		// Para kPostLoadGame, en vez de ResetToInHand() a ciegas: si
		// a_data.cycleActive es true (había un ciclo en marcha cuando se
		// guardó la partida), recupera el arma real de verdad — libera al
		// actor clavado si lo había, destruye la réplica que el propio
		// juego restauró como referencia de mundo normal, y reequipa —
		// antes de esto, esa réplica quedaba huérfana en el mundo
		// (activable, duplicando el arma que sigue en el inventario). Con
		// cycleActive a false (partida antigua sin datos de cosave, o
		// guardada con el arma ya en mano), se comporta exactamente igual
		// que ResetToInHand().
		void RecoverOrReset(const SaveCycleData& a_data);

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

		// Regreso animado (5.- RETURN, puntos 7-8): cancela el bucle de
		// tick en marcha (vuelo, o seguimiento de un actor clavado),
		// libera al objetivo si lo había, y arranca Return::BeginReturn
		// sobre la réplica ya existente. Cae a RecallWeapon (recuperación
		// instantánea) si no hay jugador o réplica válida de la que
		// partir.
		void BeginReturn();

		// Recuperación instantánea: destruye la réplica (si la hay, ver
		// Physics::DestroyReplica) y reequipa el arma de inmediato, sin
		// trayectoria de vuelta. Se mantiene como red de seguridad (cierre
		// de pantalla de carga a mitad de un regreso ya en marcha, o sin
		// jugador/réplica del que partir en BeginReturn) — el ciclo normal
		// de recuperación pasa por BeginReturn.
		void RecallWeapon();

		// Paso final común a la recuperación instantánea y a la llegada
		// del regreso animado (sin transición de captura, ver
		// ReequipAfterCapture): destruye la réplica, cancela cualquier
		// bucle de tick activo y reequipa el arma real.
		void ReequipAndReset();

		// Mejora Kratos #3 (PLAN-mejoras-kratos.md): variante de
		// ReequipAndReset para la llegada real del regreso animado
		// (Return::BeginReturn, callback onArrived) -- clona la réplica
		// visible y la engancha un instante a la mano
		// (Animation::PlayCaptureTransition) antes de completar el
		// reequipado, en vez de cortar en seco. Sin mano o réplica visible
		// de la que partir, cae en ReequipAndReset sin transición (mismo
		// comportamiento de siempre). No se usa en RecallWeapon (recall
		// instantáneo: la réplica puede estar lejísimos, sin ninguna
		// "captura" real que dramatizar, y se invoca también al cerrar
		// pantallas de carga).
		void ReequipAfterCapture();

		// Primera mitad de ReequipAndReset (inmediata): cancela el bucle de
		// tick activo y destruye la réplica, sin tocar el arma real ni el
		// estado del ciclo. Separada de ReequipActiveWeapon para que
		// ReequipAfterCapture pueda insertar la transición de captura entre
		// ambas sin duplicar la lógica de equipar.
		void DestroyReplicaKeepWeapon();

		// Segunda mitad de ReequipAndReset (el reequipado en sí, diferido
		// un tick): reequipa el arma real y limpia el estado del ciclo
		// (arma activa, vuelta a "en mano").
		void ReequipActiveWeapon();

		WeaponState weaponState;
	};
}
