// Estado del ciclo del arma -- ver WeaponState.h.

#include "3.- WEAPON/WeaponState.h"

#include "11.- SKYRIM/FirstPersonDiag.h"

namespace Weapon
{
	namespace
	{
		const char* ToString(State a_state)
		{
			switch (a_state) {
			case State::kInHand:
				return "EnMano";
			case State::kThrowing:
				return "Lanzando";
			case State::kThrown:
				return "Lanzada";
			case State::kStuck:
				return "Clavada";
			case State::kCalling:
				return "Llamando";
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
		Diag::DumpHands(std::format("estado {} -> {}", ToString(state), ToString(a_state)));
		Diag::Record(std::format("Estado del arma: {} -> {}", ToString(state), ToString(a_state)));
		state = a_state;
	}
}
