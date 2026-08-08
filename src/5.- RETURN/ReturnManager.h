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

		// Disparado desde dentro del propio bucle de tick del regreso (ver
		// BeginReturnMovement), no por un temporizador precalculado de
		// antemano -- a petición del usuario (2026-08-03): la
		// sincronización entre la animación de Atrape y el vuelo físico
		// real no es negociable. El tiempo real que falta hasta la llegada
		// se calcula de forma analítica (progreso acumulado/duración
		// prevista/ritmo del suavizado final, todos ya conocidos por el
		// propio bucle -- no una medición de velocidad, ver
		// BeginReturnMovement para el porqué de ese cambio, 2026-08-07),
		// exacto en vez de estimado -- absorbe automáticamente el
		// suavizado del tramo final (Constants::kReturnTailDistance/
		// kReturnTailMinRate, que alarga la duración real más allá de lo
		// que predice una fórmula cerrada) y no depende de que el jugador
		// se haya movido durante el regreso. Se dispara en cuanto ese
		// tiempo restante cae por debajo de Constants::kCatchAnimationLeadTime.
		// También exige que hayan pasado al menos
		// Constants::kMinCatchAnimationDelay segundos reales desde que
		// empezó el regreso (temblor de desprendimiento incluido si lo
		// hubo) -- si la distancia es tan corta que la física natural no
		// dejaría margen para ninguna de las dos cosas, Return::BeginReturn
		// alarga el temblor de desprendimiento del punto 11 lo necesario
		// (nunca ralentiza el vuelo en sí, cambio de criterio 2026-08-07,
		// ver CLAUDE.md), en vez de desacoplar animación y física. El
		// llamante debe arrancar aquí el gesto visual de Atrape
		// (WeaponManager::BeginCatchAnimation).
		//
		// Disparador independiente del enderezado visual del arma (punto
		// 10, Animation::TickSpinStraighten): ese usa el mismo tiempo
		// restante analítico pero comparado contra
		// Constants::kSpinStraightenLeadTime, deliberadamente mucho más
		// corto que kCatchAnimationLeadTime -- compartir el mismo instante
		// (como antes del 2026-08-07) hacía que la ventana de enderezado
		// consumiera la mayor parte de un regreso corto o medio, sin apenas
		// dejar ver el giro. No confundir ambos disparadores solo porque
		// comparten el mismo cálculo de tiempo restante.
		//
		// Nunca obligatorio comprobar si está asignado (mismo contrato que
		// onArrived, ver más abajo), el único llamante
		// (WeaponManager::BeginReturn) lo asigna siempre.
		std::function<void()> onApproaching;

		// El arma ha llegado a la mano del jugador (a Constants::kReturnArrivalDistance
		// o menos): la réplica se queda quieta aquí (el bucle de tick ya se
		// detiene solo). El reequipado real de Atrape NO ocurre en este
		// callback -- la anotación PIE.ThorMjolnirCatch, ya horneada en
		// Catch.hkx, marca el instante exacto en que la mano debe cerrarse
		// sobre el arma (WeaponManager::OnCatchReleaseAnimationEvent).
		// Gracias a la sincronización en vivo de onApproaching (ver más
		// arriba), ese instante debería coincidir de verdad con este --
		// confiar en la anotación en vez de en este umbral de distancia
		// para el reequipado en sí es lo que sincroniza el gesto de la
		// mano en el clip con el reequipado visual. Sí es el punto
		// correcto para una recuperación instantánea sin animación
		// (RecallWeapon).
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
	// temblor de desprendimiento (Constants::kStickShudderDuration como
	// mínimo -- BeginReturn puede alargarlo si la distancia es corta, ver
	// más abajo, ver Animation::TickShudder) sin mover la réplica, y solo
	// al terminar arranca el movimiento de vuelta descrito arriba. Si venía
	// en vuelo (kThrown), el movimiento arranca de inmediato, sin temblor.
	//
	// El movimiento de vuelta en sí siempre usa la aceleración natural
	// (Return::ComputeReturnAcceleration, acotada solo por
	// Constants::kReturnMaxDuration -- sin relación con la animación de
	// Atrape) -- cambio de criterio 2026-08-07 (ver CLAUDE.md): antes se
	// ralentizaba el propio vuelo en regresos cortos para dejar tiempo a
	// que Atrape se sincronizara, pero eso hacía que el regreso se sintiera
	// lento en distancias medias/cortas. Ahora, si hace falta más tiempo
	// del que da el vuelo natural, es el temblor de desprendimiento el que
	// se alarga por encima de su mínimo -- solo aplica si a_wasStuck es
	// true (si el regreso arranca en pleno vuelo no hay temblor que
	// alargar, caso raro de auto-recall a media parábola).
	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, bool a_wasStuck, ReturnCallbacks a_callbacks);
}
