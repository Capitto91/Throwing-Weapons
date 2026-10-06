// Estado del ciclo del arma y referencias activas (arma, réplica, actor clavado, bucle de tick).
// Lo guarda y consulta WeaponManager.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

namespace Weapon
{
	// Fases del ciclo del arma. No se guarda en el cosave.
	enum class State
	{
		kInHand,    // Equipada normalmente en la mano derecha.
		kThrowing,  // Gesto de Lanzar (Throw.hkx), arma todavía en la mano.
		kThrown,    // Lanzada: fuera de la mano.
		kStuck,     // Clavada en un enemigo o superficie.
		kCalling,   // Gesto de Llamada (Call.hkx), el regreso aún no ha empezado.
		kReturning  // Volviendo al jugador; el gesto de Atrape ocurre dentro de este estado.
	};

	class WeaponState
	{
	public:
		[[nodiscard]] State GetState() const noexcept { return state; }
		void                SetState(State a_state);

		// Arma del ciclo actual. La fija WeaponManager::PrepareThrow; se reequipa al recuperarla.
		[[nodiscard]] RE::TESBoundObject* GetActiveWeapon() const noexcept { return activeWeapon; }
		void                              SetActiveWeapon(RE::TESBoundObject* a_weapon) noexcept { activeWeapon = a_weapon; }

		// Réplica en vuelo. La fija WeaponManager::ThrowWeapon al crearla Throw::LaunchWeapon.
		[[nodiscard]] RE::ObjectRefHandle GetActiveReplicaHandle() const noexcept { return activeReplicaHandle; }
		void                              SetActiveReplicaHandle(RE::ObjectRefHandle a_handle) noexcept { activeReplicaHandle = a_handle; }

		// Actor donde está clavada la réplica (vacío si no hay). Lo fija onStuck de Throw::LaunchWeapon;
		// se libera con Combat::EndEmbeddedEffect.
		[[nodiscard]] RE::ActorHandle GetStuckActorHandle() const noexcept { return stuckActorHandle; }
		void                          SetStuckActorHandle(RE::ActorHandle a_handle) noexcept { stuckActorHandle = a_handle; }

		// Normal de la superficie donde está clavada la réplica (cero si es un actor). La fija onStuck
		// de Throw::LaunchWeapon; la usa LightningDash para el destino del desplazamiento.
		[[nodiscard]] const RE::NiPoint3& GetStuckSurfaceNormal() const noexcept { return stuckSurfaceNormal; }
		void                              SetStuckSurfaceNormal(const RE::NiPoint3& a_normal) noexcept { stuckSurfaceNormal = a_normal; }

		// Bucle de tick que mueve la réplica ahora (ida, clavada o regreso).
		// WeaponManager lo cancela desde fuera al recuperar el arma.
		[[nodiscard]] Physics::TickToken GetActiveTickToken() const noexcept { return activeTickToken; }
		void                             SetActiveTickToken(Physics::TickToken a_token) noexcept { activeTickToken = std::move(a_token); }

		// Detiene el bucle activo y olvida su token.
		void CancelTickLoop() { Physics::CancelTickLoop(activeTickToken); }

	private:
		State               state{ State::kInHand };
		RE::TESBoundObject* activeWeapon{ nullptr };
		RE::ObjectRefHandle activeReplicaHandle;
		RE::ActorHandle     stuckActorHandle;
		RE::NiPoint3        stuckSurfaceNormal{};
		Physics::TickToken  activeTickToken;
	};
}
