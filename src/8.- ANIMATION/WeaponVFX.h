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
	// todavía no existe. Si ya había un VFX activo, no lo corta primero --
	// lo solapa (coloca este antes de destruir el anterior,
	// Constants::kMovementVfxSwapOverlapDuration después, ver
	// Animation::StartOn en WeaponVFX.cpp) para que la transición no se
	// note como un reinicio. Usado así para la transición kThrowing-
	// >kThrown (ver StartMovementVFXOnReplica) desde 2026-08-10, a
	// petición del usuario.
	void StartMovementVFXOnActor(RE::Actor& a_actor);

	// Coloca el VFX sobre la réplica visual en vuelo (a_handle, ver
	// Weapon::WeaponState::GetActiveReplicaHandle) y lo sigue cada tick,
	// pegado a su nodo raíz -- pensado para
	// State::kThrown/kCalling/kReturning. Requiere que la réplica ya tenga
	// 3D cargado (Get3D() no nulo) -- a diferencia de
	// StartMovementVFXOnActor, no espera/reintenta: el llamante
	// (WeaponManager) ya solo la llama en el instante en que sabe que el 3D
	// está listo. Mismo solape que StartMovementVFXOnActor en vez de un
	// corte previo -- ver ese comentario.
	void StartMovementVFXOnReplica(RE::ObjectRefHandle a_handle);

	// Si hay un VFX continuo activo (g_activeVfxHandle, ver WeaponVFX.cpp),
	// cambia en caliente a qué posición sigue cada tick -- del objetivo que
	// tuviera antes (típicamente la réplica del arma, o su última posición
	// conocida si ya se destruyó, ver el lambda "auto-reparable" de
	// StartMovementVFXOnReplica) al hueso "WEAPON" de a_actor -- sin crear
	// ningún Activator nuevo ni tocar su secuencia (que ya está
	// reproduciéndose). A diferencia de StartMovementVFXOnActor/OnReplica
	// (vía Animation::StartOn), no hay solape con una segunda instancia --
	// es la MISMA instancia de partículas, solo cambia de qué punto del
	// mundo toma su posición cada tick, así que no hay ningún corte ni
	// reinicio que disimular. Sin efecto si no hay VFX activo (p. ej. el
	// arma ya estaba clavada y su VFX ya se apagó del todo antes de este
	// punto -- ver Weapon::WeaponManager::RecallWeapon) o si a_actor no
	// tiene el hueso "WEAPON".
	//
	// Pensada para el instante del reequipado real durante el gesto de
	// Atrape (Weapon::WeaponManager::PerformCatchReequip, vía
	// ReequipAndReset(a_reattachVfxToHand=true)) -- a petición del usuario
	// (2026-08-10): "quería que las chispas siguieran al arma [durante el
	// resto de la animación] y no nacieran de la posición donde se produce
	// el catch". Sin esto, el VFX se quedaba siguiendo la posición de la
	// réplica ya destruida (capturada a mitad del gesto de Atrape, no la
	// pose final de reposo en la mano) durante todo el margen hasta
	// Weapon::WeaponManager::FinishCatchAnimation -- las chispas del fundido
	// (Animation::FadeOutMovementVFX) nacían flotando en el aire, en vez de
	// desde el arma ya visible en la mano.
	void RetargetMovementVFXToActor(RE::Actor& a_actor);

	// Corta el VFX activo (si lo hay) de golpe: cancela sus bucles de tick
	// (Physics::CancelTickLoop) y borra la referencia colocada (mismo
	// mecanismo que Physics::DestroyReplica -- Disable + SetDelete).
	// Cancela también cualquier espera de carga de 3D todavía pendiente de
	// StartMovementVFXOnActor/OnReplica/FadeOutMovementVFX. Sin efecto si
	// no había ningún VFX activo. Ver FadeOutMovementVFX para el corte que
	// no es de golpe -- varios intentos previos descartados antes de
	// llegar a ese diseño, ver CHANGELOG.md v1.14.8 a v1.14.20.
	void StopMovementVFX();

	// Diseño final (2026-08-10, ver CHANGELOG.md v1.14.8 a v1.14.20 para
	// todos los intentos previos descartados -- todos compartían el mismo
	// problema de fondo: intentar apagar la emisión de partículas de una
	// instancia ya en marcha, desde fuera, nunca resultó fiable con
	// ninguna API disponible). Este ya no lo intenta -- en vez de tocar
	// nada del Activator continuo (Constants::kMovementVfxActivatorLocalFormID),
	// coloca uno nuevo, "de un solo uso"
	// (Constants::kMovementVfxOffActivatorLocalFormID -- .nif con una
	// única secuencia, CYCLE_CLAMP, BirthRate cae a 0 por su propia curva
	// de keyframes) en la misma posición donde estaba el continuo, y lo
	// deja apagarse solo según su propia animación -- nunca hace falta
	// decirle nada más desde C++ una vez colocado. El continuo se destruye
	// Constants::kMovementVfxSwapOverlapDuration después, no de
	// inmediato, para que los dos coexistan un instante y no haya ningún
	// frame sin partículas visibles durante el relevo. El "de un solo uso"
	// se destruye de verdad pasado Constants::kMovementVfxFadeOutSafetyMargin
	// (cubre su ciclo completo). Guardado por generación, mismo patrón que
	// WaitFor3DThenStartTicking: si otro Start/Stop corre antes de que
	// venza cualquiera de los dos márgenes, ese cierre se descarta en
	// silencio. Sin efecto si no hay VFX activo.
	//
	// Reentrante a propósito (2026-08-10, fix de un bug reportado por el
	// usuario -- una segunda llamada mientras el "de un solo uso" ya
	// colocado por una primera todavía se estaba apagando por su cuenta
	// colocaba OTRO burst encima, viéndose como una tanda de chispas
	// tardía y desincronizada): si ya hay un burst en marcha, una llamada
	// nueva es un no-op silencioso -- deja que el que ya está en marcha
	// termine solo, sin apilar ninguno nuevo. Puede llamarse de más sin
	// riesgo -- por ejemplo Weapon::WeaponManager::RecallWeapon la llama
	// incondicionalmente, sepa o no si el arma ya estaba clavada y
	// apagándose desde antes (ver onStuck más abajo).
	//
	// Quién y cuándo la llama (2026-08-10, a petición del usuario -- antes
	// se disparaba en el instante del reequipado real, cortando la
	// animación de Atrape a medias): el llamante decide el momento exacto,
	// esta función no asume nada. En el flujo normal de Atrape, el único
	// disparador es Weapon::WeaponManager::FinishCatchAnimation, al final
	// de verdad de la animación completa -- Weapon::WeaponManager::
	// ReequipAndReset (llamada antes, al reequipar el arma real) ya NO
	// dispara el fundido por su cuenta; en su lugar, reengancha el VFX al
	// hueso "WEAPON" del jugador (ver RetargetMovementVFXToActor) para que
	// siga la mano durante el resto del gesto en vez de quedarse fijo en el
	// punto del catch, así que al llegar aquí, el fundido nace de la
	// posición real del arma ya en la mano. Los caminos de recuperación
	// instantánea, sin ninguna animación de por medio (BeginCatchAnimation/
	// BeginReturn sin jugador o réplica, RecallWeapon), la llaman a mano
	// justo después de ReequipAndReset -- esos no reenganchan al hueso (no
	// hay gesto que seguir), así que usan la última posición conocida de
	// la réplica (ver el lambda "auto-reparable" de
	// StartMovementVFXOnReplica). El caso kThrown->kStuck (onStuck,
	// embebido instantáneo) sigue llamándola directamente también, sin
	// pasar por ReequipAndReset -- y, gracias a la reentrancia de arriba,
	// ya no hace falta ningún cierre de emergencia aparte en
	// OnLoadingScreenClosed para el caso en que la pantalla de carga
	// interrumpe mientras el gesto de Atrape ya estaba en marcha (ver
	// ese comentario): el mismo RecallWeapon que ya reequipa el arma en
	// ese caso llama también a esta función, y si ya había un burst en
	// marcha desde antes, simplemente no hace nada más.
	//
	// a_extraSettleDelay (2026-08-10, a petición del usuario -- solo
	// Weapon::WeaponManager::FinishCatchAnimation pasa true): si true,
	// no captura la posición todavía -- espera
	// Constants::kCatchVfxSettleDelay más (guardado por generación, mismo
	// patrón que el resto del archivo: si algo más releva el VFX mientras
	// tanto, este cierre se descarta en silencio) antes de ejecutar el
	// cuerpo real de la función. El VFX sigue activo y siguiendo la mano
	// durante todo ese margen extra (RetargetMovementVFXToActor no se
	// toca todavía) -- pensado para el hueco real entre el evento
	// attackStop (que dispara FinishCatchAnimation) y el instante en que
	// el grafo de animación vanilla termina de verdad su propia mezcla
	// (blend) hacia la pose de reposo: capturar la posición demasiado
	// pronto (en el instante de attackStop) dejaba el burst fijo en un
	// punto que la mano abandonaba un instante después -- se veía como un
	// chorro fuera de sitio, aparecido menos de medio segundo tras el
	// final aparente de la animación. Por defecto false (el resto de
	// llamantes no tienen ninguna mezcla de animación que esperar).
	void FadeOutMovementVFX(bool a_extraSettleDelay = false);
}
