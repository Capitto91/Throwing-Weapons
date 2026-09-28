// Efectos visuales durante los power attacks cuerpo a cuerpo con el arma
// arrojadiza en la mano (2026-09-28, a petición del usuario): las mismas
// chispas (Animation::WeaponVFX) y el mismo destello con luz
// (Animation::WeaponGlow) que acompañan al lanzamiento. Puro polish, sin
// punto numerado en Mecanica del arma.txt (no cubre los ataques cuerpo a
// cuerpo).
//
// Se hace desde código y no con una habilidad de la Creation Kit con la
// condición IsPowerAttacking: las condiciones de un efecto activo no se
// reevalúan cada fotograma, así que el efecto podía llegar tarde para un
// golpe de ~1s, y un Magic Effect no puede reutilizar los Activators del
// destello/chispas (luz dinámica real, fundidos, burst de apagado).
//
// Señal: un sink de BSAnimationGraphEvent sobre los grafos del jugador
// (mismo patrón que Animation::AttackAnimType). En
// Constants::kPowerAttackVfxStartEvent, si Actor::IsPowerAttacking y el
// ciclo está en reposo (State::kInHand, arma arrojadiza en la mano), se
// encienden los dos efectos siguiendo el hueso "WEAPON"; en
// Constants::kPowerAttackVfxStopEvent se apagan con su fundido normal
// (Animation::FadeOutMovementVFX / StopWeaponGlow). Si attackStop no llega,
// Constants::kPowerAttackVfxSafetyTimeout los apaga igual.
//
// Los dos módulos de efectos son de instancia única, compartida con el
// ciclo de lanzamiento: WeaponManager::BeginThrowAnimation llama a Cancel
// antes de arrancar los suyos.
//
// Solo el jugador, igual que AttackAnimType.

#pragma once

namespace RE
{
	class Actor;
}

namespace Animation::PowerAttackVFX
{
	// Registra el sink en todos los grafos de a_actor (idempotente).
	// Llamar en los mismos puntos que AttackAnimType::EnsureRegistered:
	// al equipar el arma, tras cargar partida y al cerrar una pantalla de
	// carga (el grafo puede haberse recreado).
	void EnsureRegistered(RE::Actor& a_actor);

	// Da por terminados los efectos del power attack en curso, si los hay,
	// apagando el destello con su fundido. Las chispas se dejan tal cual:
	// el llamante va a arrancar las suyas, que se solapan con estas sin
	// corte (ver Animation::StartMovementVFXOnActor). Hilo principal.
	void Cancel();
}
