// Regreso de la réplica a la mano: temblor si estaba clavada, curva con aceleración creciente
// y avisos para el gesto de Atrape y la llegada. Lo arranca WeaponManager::BeginReturn.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

#include <functional>

namespace Return
{
	struct ReturnCallbacks
	{
		// Token del bucle del regreso, para cancelarlo desde fuera.
		std::function<void(Physics::TickToken)> onTickStarted;

		// Aviso cuando faltan Constants::kCatchAnimationLeadTime segundos para llegar.
		// WeaponManager arranca aquí el gesto de Atrape.
		std::function<void()> onApproaching;

		// El arma ha llegado a la mano (Constants::kReturnArrivalDistance); la réplica se detiene.
		// El reequipado lo hace la anotación de Catch.hkx, no este aviso.
		std::function<void()> onArrived;
	};

	// Inicia el regreso de a_replicaHandle a la mano de a_player (curva que sigue a la mano y giro).
	// Con a_wasStuck, primero tiembla; el temblor se alarga si hace falta tiempo para el Atrape.
	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, bool a_wasStuck, ReturnCallbacks a_callbacks);
}
