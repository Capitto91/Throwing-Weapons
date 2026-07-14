// Define el estado actual del arma durante su ciclo de uso.
// Guarda referencias, estados internos y datos necesarios para saber dónde está
// el arma y qué está ocurriendo con ella.

#pragma once

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

	private:
		State                state{ State::kInHand };
		RE::TESBoundObject*  activeWeapon{ nullptr };
		RE::ObjectRefHandle  activeReplicaHandle;
	};
}
