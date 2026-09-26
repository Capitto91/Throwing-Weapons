// Vigilante de eventos del grafo de animación para el corte de ataque en
// curso de WeaponManager::InterruptAttackThen (2026-09-26).
//
// Tras nuestro "attackStop", el "attackStart" siguiente solo arranca
// Throw.hkx/Call.hkx cuando el grafo ha terminado de mezclar de vuelta a
// reposo -- ver el bloque "Cortar un ataque en curso" de Constants.h para
// las medidas. La señal usada es el N-ésimo "attackStop" que emite el grafo
// desde el corte (Constants::kAttackInterruptReadyEventOrdinal): el primero
// es el eco inmediato del nuestro (+12ms), el segundo llega al terminar la
// mezcla (+195ms medido con BFCO).
//
// Mientras está armado registra además en el log todos los eventos del
// grafo con los ms transcurridos (diagnóstico, heredado de la sonda
// AttackInterruptProbe) -- pendiente de silenciar antes de publicar.
#pragma once

#include <functional>

namespace Events::AttackInterruptWatcher
{
	// Registra el sink en el grafo de a_actor (idempotente, ver
	// Actor::AddAnimationGraphEventSink) y arma el vigilante a partir de
	// este instante, sustituyendo cualquier armado anterior. a_onReady se
	// ejecuta una sola vez, en el hilo principal (vía
	// SKSE::GetTaskInterface()->AddTask -- los eventos del grafo llegan
	// desde los hilos de animación), al recibir el evento de "mezcla
	// terminada"; nunca si no llega dentro de
	// Constants::kAttackInterruptWatchWindow (el llamante debe tener su
	// propia red de seguridad). Llamar justo antes de disparar "attackStop".
	void Arm(RE::Actor& a_actor, std::function<void()> a_onReady);

	// Desarma el vigilante sin ejecutar el callback pendiente.
	void Disarm();
}
