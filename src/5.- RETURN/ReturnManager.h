// Controla el regreso del arma hacia el jugador.
// Gestiona inicio del retorno, seguimiento de trayectoria y recuperación final.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

#include <functional>

// Regreso sobre la propia réplica que ya existe desde el lanzamiento (nunca
// se recrea, a diferencia de una iteración anterior descartada — ver
// CHANGELOG.md y CLAUDE.md): el llamante ya ha cancelado cualquier bucle de
// tick previo (vuelo, o seguimiento de un actor clavado) antes de invocar
// BeginReturn, así que la posición actual de la réplica es el punto de
// partida real del regreso.

namespace Return
{
	struct ReturnCallbacks
	{
		// Token del bucle de tick del regreso (ver Physics::TickToken). El
		// llamante debe guardarlo para poder cancelarlo si hace falta
		// (p. ej. cierre de pantalla de carga a mitad del trayecto).
		std::function<void(Physics::TickToken)> onTickStarted;

		// El arma ha llegado a la mano del jugador (a Constants::kReturnArrivalDistance
		// o menos): el llamante debe reequiparla y destruir la réplica.
		std::function<void()> onArrived;
	};

	// Inicia el regreso de a_replicaHandle hacia la mano de a_player:
	// trayectoria curva (Bezier cuadrática, punto 7) con aceleración
	// híbrida partiendo del reposo (punto 8). La distancia inicial (para
	// la aceleración) y el punto de control de la curva se calculan una
	// única vez, capturando la posición de la mano en este instante; el
	// extremo final de la curva se sigue actualizando cada tick a la
	// posición actual de la mano, para no perder de vista al jugador si
	// se mueve durante el regreso. El giro (punto 10) se sigue calculando
	// a mano cada tick igual que en la ida (ver
	// 8.- ANIMATION/WeaponAnimation::TickSpin) -- sin ningún "reanudar"
	// especial: si la réplica venía de kStuck, el giro simplemente
	// continúa desde el ángulo en el que se quedó congelada.
	//
	// a_wasStuck (punto 11): si el arma estaba clavada (superficie o
	// actor, el llamante ya liberó a este último antes de llegar aquí)
	// justo antes de pulsar recuperar, primero se reproduce un breve
	// temblor de desprendimiento (Constants::kStickShudderDuration, ver
	// Animation::TickShudder) sin mover la réplica, y solo al terminar
	// arranca el movimiento de vuelta descrito arriba. Si venía en vuelo
	// (kThrown), el movimiento arranca de inmediato, sin temblor.
	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, bool a_wasStuck, ReturnCallbacks a_callbacks);
}
