// Implementación de la gestión del estado del arma.
// Controla cambios entre estados como equipada, lanzada, clavada o regresando.

#include "3.- WEAPON/WeaponState.h"

namespace Weapon
{
	namespace
	{
		const char* ToString(State a_state)
		{
			switch (a_state) {
			case State::kInHand:
				return "EnMano";
			case State::kAiming:
				return "Apuntando";
			case State::kThrown:
				return "Lanzada";
			case State::kStuck:
				return "Clavada";
			case State::kReturning:
				return "Regresando";
			default:
				return "Desconocido";
			}
		}
	}

	void WeaponState::SetState(State a_state)
	{
		if (a_state == state) {
			return;
		}

		logs::info("Estado del arma: {} -> {}", ToString(state), ToString(a_state));
		state = a_state;
	}
}
