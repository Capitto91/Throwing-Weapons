// Define el estado actual del arma durante su ciclo de uso.
// Guarda referencias, estados internos y datos necesarios para saber dónde está
// el arma y qué está ocurriendo con ella.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

namespace Weapon
{
	// Ciclo de vida del arma arrojadiza única (Mecanica del arma.txt,
	// punto 1). kStuck/kReturning se declaran ya como parte del modelo
	// completo, pero hasta que 5.- RETURN exista (Fase 6) el ciclo real
	// solo transita entre kInHand/kAiming/kThrown, con recuperación
	// instantánea al pulsar el botón (aunque la ida en sí ya vuela de
	// verdad desde la Fase 3, ver WeaponManager).
	enum class State
	{
		kInHand,    // Equipada normalmente en la mano derecha.
		kAiming,    // Botón pulsado, apuntando antes de soltar.
		kThrown,    // Lanzada: fuera de la mano.
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

		// Réplica visual en vuelo durante el ciclo actual (ver
		// Physics::SpawnReplica, nunca un RE::Projectile). La fija
		// WeaponManager::ThrowWeapon en cuanto Throw::LaunchWeapon la crea;
		// RecallWeapon la usa para destruirla al recuperar el arma.
		[[nodiscard]] RE::ObjectRefHandle GetActiveReplicaHandle() const noexcept { return activeReplicaHandle; }
		void                              SetActiveReplicaHandle(RE::ObjectRefHandle a_handle) noexcept { activeReplicaHandle = a_handle; }

		// Actor en el que quedó clavada la réplica (punto 6), vacío si no
		// hay ninguno (superficie, o todavía en vuelo). Lo fija el
		// callback onStuck de Throw::LaunchWeapon; RecallWeapon lo usa
		// para liberar al objetivo (Combat::EndEmbeddedEffect) antes de
		// olvidarlo.
		[[nodiscard]] RE::ActorHandle GetStuckActorHandle() const noexcept { return stuckActorHandle; }
		void                          SetStuckActorHandle(RE::ActorHandle a_handle) noexcept { stuckActorHandle = a_handle; }

		// Token del bucle de tick que controla la réplica en este instante
		// (vuelo de ida, seguimiento de un actor clavado, o regreso), sea
		// cual sea el módulo que lo haya arrancado. Es lo único que
		// permite a WeaponManager cancelar ese bucle desde fuera (p. ej.
		// al pulsar el botón de recuperar mientras el arma sigue en
		// marcha) sin tener que destruir la réplica — ver
		// Physics::TickToken.
		[[nodiscard]] Physics::TickToken GetActiveTickToken() const noexcept { return activeTickToken; }
		void                             SetActiveTickToken(Physics::TickToken a_token) noexcept { activeTickToken = std::move(a_token); }

	private:
		State               state{ State::kInHand };
		RE::TESBoundObject* activeWeapon{ nullptr };
		RE::ObjectRefHandle activeReplicaHandle;
		RE::ActorHandle     stuckActorHandle;
		Physics::TickToken  activeTickToken;
	};
}
