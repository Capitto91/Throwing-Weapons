// Destello visual (y, más adelante, luz real) que acompaña al arma desde
// que empieza el gesto de Lanzar hasta que se completa el Atrape. Punto
// "polish" nuevo, sin número en Mecanica del arma.txt (mismo criterio que
// 8.- ANIMATION/WeaponVFX.h para las chispas).
//
// Arquitectura: un .nif propio y separado (Constants::kGlowEffectPath,
// "ThorMjolnirLight.nif", solo efectos visuales -- decisión del usuario
// 2026-08-27, sustituye a un primer intento de esta misma sesión que
// horneaba la malla directamente en el NIF del arma) colocado como
// Activator real (Constants::kWeaponGlowActivatorLocalFormID, sin script)
// vía PlaceObjectAtMe y seguido cada tick -- mismo mecanismo de control
// manual ya probado en todo el proyecto (ver WeaponVFX.h para las
// chispas), en vez de BSTempEffectParticle (como WeaponTrail): un
// BSTempEffectParticle tiene una vida útil FIJA fijada al crearlo, que no
// cubre "hasta que se hace el Atrape" -- el arma puede quedar clavada un
// tiempo indefinido antes de llamarla de vuelta, y un Activator colocado
// con PlaceObjectAtMe no caduca por sí solo.
//
// Diferencia deliberada frente a WeaponVFX.h: aquí NUNCA se recrea la
// referencia al cambiar de qué sigue (mano del jugador <-> réplica en
// vuelo) -- se coloca UNA sola vez al empezar (StartWeaponGlow) y se
// destruye UNA sola vez al terminar (StopWeaponGlow); entre medias, los
// cambios de objetivo son un simple retargeteo del bucle de tick
// (RetargetWeaponGlowToReplica/RetargetWeaponGlowToActor), sin solape ni
// generación de swap que gestionar. WeaponVFX.h necesita ese aparato
// porque además alterna entre dos .nif distintos (continuo/de un solo
// uso, para poder apagarse solo); aquí siempre es el mismo .nif durante
// todo el ciclo, así que un retargeteo puro basta y no hay ningún hueco
// visual que evitar (nunca se destruye nada a medio camino, solo al
// final de verdad).
//
// Pendiente (segunda mitad de esta funcionalidad, decisión del usuario
// 2026-08-27 de hacerlo por partes): la luz real
// (Constants::kWeaponGlowLightLocalFormID, ya creada en la Creation Kit)
// todavía no está conectada -- de momento este archivo solo cuelga y
// sigue la malla visual. Se añadirá sobre este mismo mecanismo (un
// RE::NiPointLight adjunto al mismo Activator, técnica de
// powerof3/LightPlacer ya verificada contra los headers de este proyecto)
// una vez confirmado en el juego que el seguimiento funciona.

#pragma once

namespace Animation
{
	// Coloca Constants::kGlowEffectPath sobre a_actor (PlaceObjectAtMe) y
	// arranca un bucle de tick que lo sigue pegado al hueso "WEAPON" de su
	// esqueleto -- pensada para el instante exacto en que empieza la
	// animación de Lanzar (WeaponManager::BeginThrowAnimation), con el
	// arma real todavía en la mano. El 3D de una referencia recién
	// colocada tarda unos frames en cargar en segundo plano -- se
	// reintenta con el mismo patrón hilo-que-duerme-y-reencola del resto
	// del proyecto hasta que esté listo, sin bloquear nada mientras tanto.
	//
	// Sin efecto (con aviso en el log) si ya había un destello activo (no
	// debería poder pasar -- solo hay un ciclo de arma a la vez en todo el
	// plugin) o si Constants::kWeaponGlowActivatorLocalFormID no resuelve
	// a un Activator real (placeholder sin rellenar todavía, o FormID
	// equivocado).
	void StartWeaponGlow(RE::Actor& a_actor);

	// Cambia en caliente qué posición sigue el destello ya colocado (ver
	// StartWeaponGlow) -- de la mano del jugador a la réplica en vuelo,
	// SIN recolocar nada ni tocar ninguna secuencia. Pensada para el
	// instante en que la réplica ya tiene un handle real
	// (WeaponManager::ThrowWeapon, callback onSpawned) -- si a_handle
	// todavía no resuelve a una referencia con 3D cargado, no hace nada (a
	// diferencia de StartWeaponGlow, no espera/reintenta -- mismo criterio
	// que Animation::StartMovementVFXOnReplica). "Auto-reparable" igual
	// que esa función: si la réplica deja de existir mientras este
	// retargeteo sigue activo, el destello se congela en su última
	// posición conocida en vez de saltar al origen del mundo.
	//
	// Sin efecto si no hay ningún destello activo.
	void RetargetWeaponGlowToReplica(RE::ObjectRefHandle a_handle);

	// Contraparte de RetargetWeaponGlowToReplica -- de la réplica de
	// vuelta al hueso "WEAPON" del jugador, sin recolocar nada. Pensada
	// para el instante del reequipado real durante el Atrape
	// (WeaponManager::ReequipAndReset, a_reattachVfxToHand=true), antes de
	// que la réplica se destruya -- mismo motivo que
	// Animation::RetargetMovementVFXToActor: que el destello siga la mano
	// real durante el resto del gesto en vez de quedarse fijo en el punto
	// del catch.
	//
	// Sin efecto si no hay ningún destello activo, o si a_actor no tiene
	// el hueso "WEAPON".
	void RetargetWeaponGlowToActor(RE::Actor& a_actor);

	// Corta el destello de golpe: cancela el bucle de tick y borra la
	// referencia colocada (mismo mecanismo que Physics::DestroyReplica --
	// Disable + SetDelete). Pensada para WeaponManager::FinishCatchAnimation
	// (final de verdad del Atrape animado, tras RetargetWeaponGlowToActor)
	// y para los caminos de recuperación instantánea sin animación de por
	// medio (BeginCatchAnimation/BeginReturn sin jugador o réplica,
	// RecallWeapon), llamada justo después de ReequipAndReset en esos
	// casos -- mismo patrón que Animation::FadeOutMovementVFX/StopMovementVFX,
	// sin el fundido (aquí no hace falta: no hay un segundo .nif "de un
	// solo uso" que tome el relevo, un corte directo basta).
	//
	// Sin efecto si no había ningún destello activo.
	void StopWeaponGlow();
}
