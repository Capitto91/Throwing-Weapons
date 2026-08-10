// Enciende/apaga el VFX de chispas mientras el arma se mueve de verdad
// (apuntando, lanzando, en vuelo, llamando o volviendo -- nunca en reposo en
// la mano ni clavada, ver Weapon::WeaponManager::TransitionState). Puro
// polish, sin punto numerado en Mecanica del arma.txt.
//
// Coloca un Activator real (Constants::kMovementVfxActivatorLocalFormID, vía
// RE::TESObjectREFR::PlaceObjectAtMe -- mismo patrón que Physics::SpawnReplica)
// y lo sigue con el mismo patrón de control manual por tick que ya mueve la
// réplica del arma en todo el proyecto (SetPosition + Physics::SyncHavok +
// Update3DPosition cada tick, ver Physics::StartTickLoop) -- tercer intento,
// tras dos descartados (ver CHANGELOG.md):
//   1) RE::BSTempEffectParticle::Spawn -- el modelo cargaba de verdad
//      (particleObject confirmado no nulo) pero nunca llegó a renderizar
//      nada, con ningún valor de a_flags/escala probado.
//   2) RE::NiNode::AttachChild sobre el hueso/nodo objetivo -- probado
//      incluso con un objeto garantizado bueno (copia del arma equipada, el
//      mismo tipo de objeto que Physics::SpawnReplica ya renderiza sin
//      problema en todo el proyecto): tampoco se vio nada. Descarta que el
//      problema fuera el Activator/NIF de chispas -- apunta a que
//      AttachChild no basta por sí solo para que el motor trate la
//      referencia como visible/en su sitio a efectos de renderizado o
//      culling, solo mueve el nodo 3D en sí.
// Esta tercera vía reutiliza exactamente el mecanismo ya probado y fiable en
// todo el proyecto para la réplica del arma, sin ninguna API nueva sin
// precedente.

#pragma once

namespace Animation
{
	// Coloca el VFX en a_actor y lo sigue cada tick, pegado al hueso
	// "WEAPON" de su esqueleto (existe siempre, ver
	// GetHandBoneWorldRotation) -- pensado para State::kAiming/kThrowing,
	// donde el arma todavía está físicamente en la mano y la réplica
	// todavía no existe. Corta primero cualquier VFX ya activo (ver
	// StopMovementVFX) antes de arrancar este.
	void StartMovementVFXOnActor(RE::Actor& a_actor);

	// Coloca el VFX sobre la réplica visual en vuelo (a_handle, ver
	// Weapon::WeaponState::GetActiveReplicaHandle) y lo sigue cada tick,
	// pegado a su nodo raíz -- pensado para
	// State::kThrown/kCalling/kReturning. Requiere que la réplica ya tenga
	// 3D cargado (Get3D() no nulo) -- a diferencia de
	// StartMovementVFXOnActor, no espera/reintenta: el llamante
	// (WeaponManager) ya solo la llama en el instante en que sabe que el 3D
	// está listo. Corta primero cualquier VFX ya activo.
	void StartMovementVFXOnReplica(RE::ObjectRefHandle a_handle);

	// Corta el VFX activo (si lo hay): cancela su bucle de tick
	// (Physics::CancelTickLoop) y borra la referencia colocada (mismo
	// mecanismo que Physics::DestroyReplica -- Disable + SetDelete).
	// Cancela también cualquier espera de carga de 3D todavía pendiente de
	// StartMovementVFXOnActor/OnReplica. Sin efecto si no había ningún VFX
	// activo.
	//
	// Intento descartado (2026-08-10, ver CHANGELOG.md): dejar que las
	// partículas ya nacidas murieran solas por su cuenta, apagando en vivo
	// el modificador emisor (RE::NiPSysModifier::SetActive(false) sobre el
	// de ORDER::kEmitter) en vez de cortar de golpe -- confirmado con log
	// real del juego que el flag `active` se pone correctamente, pero el
	// motor de Bethesda no lo usa para decidir si el emisor nace
	// partículas nuevas (siguieron naciendo igual). Revertido a este corte
	// inmediato.
	void StopMovementVFX();
}
