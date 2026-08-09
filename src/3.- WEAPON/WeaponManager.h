// Controlador principal del arma original.
// Gestiona ocultar, desactivar, activar y restaurar el arma física del jugador.

#pragma once

#include "3.- WEAPON/WeaponState.h"

namespace Weapon
{
	class WeaponManager
	{
	public:
		// Datos mínimos para reconstruir el ciclo si la partida se guardó a
		// medias (p. ej. a mitad de un lanzamiento) — ver Events::Init,
		// registro del cosave. FormID en vez de handles: los handles no
		// sobreviven a un guardado/carga, los FormID sí (remapeados con
		// SerializationInterface::ResolveFormID).
		struct SaveCycleData
		{
			bool       cycleActive{ false };
			RE::FormID weaponFormID{ 0 };
			RE::FormID replicaFormID{ 0 };
			RE::FormID stuckActorFormID{ 0 };
		};

		static WeaponManager* GetSingleton();

		WeaponManager(const WeaponManager&) = delete;
		WeaponManager(WeaponManager&&) = delete;
		WeaponManager& operator=(const WeaponManager&) = delete;
		WeaponManager& operator=(WeaponManager&&) = delete;

		// Llamados desde Input::InputManager cuando se pulsa/suelta el botón
		// de apuntar. Deciden, según el estado actual, si hay que empezar a
		// apuntar, lanzar el arma o recuperarla.
		void OnAimButtonDown();
		void OnAimButtonUp();

		[[nodiscard]] State GetState() const noexcept { return weaponState.GetState(); }

		// Arma comprometida con el ciclo actual (nullptr si está "en
		// mano"). La usarán los módulos de detección de impacto a partir
		// de la Fase 4/5 para identificar si un golpe lo causó nuestra
		// réplica.
		[[nodiscard]] RE::TESBoundObject* GetActiveWeapon() const noexcept { return weaponState.GetActiveWeapon(); }

		// Fuerza la vuelta a "en mano" sin tocar el arma física, olvidando
		// cualquier arma activa. Se usa al cargar/empezar partida: tras
		// reiniciar el proceso no hay forma fiable de saber si el arma no
		// equipada es por nuestro ciclo o por decisión del jugador (nunca
		// la equipó, la vendió...), así que no se fuerza nada y se deja tal
		// cual está en el guardado.
		void ResetToInHand();

		// Para el callback de guardado del cosave: qué persistir del ciclo
		// actual en este instante.
		[[nodiscard]] SaveCycleData CaptureSaveData() const;

		// Para kPostLoadGame, en vez de ResetToInHand() a ciegas: si
		// a_data.cycleActive es true (había un ciclo en marcha cuando se
		// guardó la partida), recupera el arma real de verdad — libera al
		// actor clavado si lo había, destruye la réplica que el propio
		// juego restauró como referencia de mundo normal, y reequipa —
		// antes de esto, esa réplica quedaba huérfana en el mundo
		// (activable, duplicando el arma que sigue en el inventario). Con
		// cycleActive a false (partida antigua sin datos de cosave, o
		// guardada con el arma ya en mano), se comporta exactamente igual
		// que ResetToInHand().
		void RecoverOrReset(const SaveCycleData& a_data);

		// Si el ciclo está en marcha (apuntando o lanzada), recupera el
		// arma de inmediato (incluye destruir la réplica en vuelo si la
		// hay). Pensado para cuando se cierra una pantalla de carga
		// (puerta, viaje rápido...).
		void OnLoadingScreenClosed();

		// Llamado desde Events::OARFunctions::ThrowReleaseFunction::RunImpl
		// (ver 10.- EVENTS/OARFunctions.h/.cpp) -- función custom registrada
		// en la API de Open Animation Replacer, invocada directamente por
		// OAR (sin ningún BSAnimationGraphEvent de por medio) en el instante
		// exacto de la anotación de liberación mientras se reproduce
		// Throw.hkx -- o desde la red de seguridad por tiempo si esa
		// anotación nunca llega (ver Constants::kThrowReleaseFallbackWindow).
		// Sin efecto si el estado ya cambió por otra vía (p. ej. una
		// pantalla de carga) antes de que llegara.
		void OnThrowReleaseAnimationEvent();

		// Mismo patrón que OnThrowReleaseAnimationEvent, para Llamada:
		// llamado desde Events::OARFunctions::CallReleaseFunction::RunImpl
		// mientras se reproduce Call.hkx -- o desde la red de seguridad por
		// tiempo si esa anotación nunca llega (ver
		// Constants::kCallReleaseFallbackWindow). También dispara el sonido
		// del chasquido (Audio::PlayReliableOneShot) y arranca
		// Return::BeginReturn -- ambos deben ocurrir exactamente en este
		// instante (sincronizados con la anotación real). Sin efecto si el
		// estado ya cambió por otra vía antes de que llegara.
		//
		// A diferencia de antes (2026-08-08, ver CLAUDE.md/
		// Constants::kCallAnimationTailDuration), ya NO desatasca el grafo
		// aquí mismo -- eso lo hace FinishCallAnimation, diferida ese
		// margen, para no cortar la cola visual del clip.
		void OnCallReleaseAnimationEvent();

		// Diferida desde OnCallReleaseAnimationEvent (ver
		// Constants::kCallAnimationTailDuration): desatasca el grafo
		// (Constants::kAttackStopAnimationEvent) y suelta el bloqueo de
		// movimiento/AnimationDriven/el trigger de OAR de Llamada, una vez
		// que la cola visual de Call.hkx ya ha tenido tiempo de terminar.
		// callAnimationActive (con el mismo papel que catchAnimationActive
		// para Atrape) trackea si esta limpieza sigue pendiente,
		// independiente de weaponState -- comprobado también en
		// OnLoadingScreenClosed/ResetToInHand por si una pantalla de carga
		// interrumpe justo durante este margen de espera.
		void FinishCallAnimation();

		// Llamado desde Events::OARFunctions::CatchReleaseFunction::RunImpl,
		// ya horneada en Catch.hkx desde el principio, exactamente Constants::kCatchAnimationLeadTime
		// después del arranque de la animación -- medido por el usuario
		// sobre el propio clip) mientras se reproduce Catch.hkx -- o desde
		// la red de seguridad por tiempo si esa anotación nunca llega
		// (Constants::kCatchReleaseFallbackWindow). Esta anotación, no la
		// llegada física en sí, es la que gatea el reequipado real
		// (ReequipAndReset, ver PerformCatchReequip) y el temblor de
		// cámara -- marca el instante exacto en que la mano se cierra sobre
		// el arma en el propio clip. Sin efecto si el gesto ya se cerró por
		// otra vía (catchAnimationActive a false) o si el reequipado ya se
		// disparó por otra vía (catchReequipDone) antes de que llegara --
		// esto último para que la red de seguridad por tiempo no repita el
		// reequipado si la anotación real llega tarde mientras ya está en
		// marcha.
		//
		// Cambio de criterio (2026-08-08, a petición del usuario, ver
		// CLAUDE.md/catchPhysicallyArrived): esta anotación tiene
		// temporización fija, calculada sobre una predicción de cuándo va
		// a llegar la réplica -- en regresos largos esa predicción puede
		// quedarse corta (confirmado con logs reales del juego). Si la
		// llegada física real todavía no se ha confirmado
		// (catchPhysicallyArrived), el reequipado se difiere
		// (catchReequipPending) hasta que OnPhysicalArrival la confirme, en
		// vez de reequipar a ciegas mientras la réplica sigue visiblemente
		// en vuelo.
		//
		// A diferencia de antes (2026-08-08, ver CLAUDE.md/
		// Constants::kCatchAnimationTailDuration), ya NO desatasca el grafo
		// aquí mismo -- eso lo hace FinishCatchAnimation, diferida ese
		// margen, para no cortar la cola visual del clip.
		void OnCatchReleaseAnimationEvent();

		// Llamado desde Return::ReturnCallbacks::onArrived (ver
		// WeaponManager::BeginReturn): confirma que la réplica ha llegado
		// de verdad a la mano, físicamente -- si OnCatchReleaseAnimationEvent
		// ya había querido reequipar antes de esta confirmación
		// (catchReequipPending), completa aquí el reequipado diferido.
		void OnPhysicalArrival();

		// Cuerpo real del reequipado del gesto de Atrape (temblor de
		// cámara + ReequipAndReset + arranque del margen de
		// Constants::kCatchAnimationTailDuration) -- extraído de
		// OnCatchReleaseAnimationEvent para poder llamarlo tanto de
		// inmediato (caso normal) como diferido, desde OnPhysicalArrival
		// (ver catchReequipPending).
		void PerformCatchReequip();

		// Diferida desde PerformCatchReequip (ver
		// Constants::kCatchAnimationTailDuration): desatasca el grafo
		// (Constants::kAttackStopAnimationEvent) y suelta el bloqueo de
		// movimiento/AnimationDriven/CatchTrigger, una vez que la cola
		// visual de Catch.hkx ya ha tenido tiempo de terminar. Sin efecto
		// si el gesto ya se cerró por otra vía (catchAnimationActive a
		// false, p. ej. una pantalla de carga a mitad de este margen de
		// espera, ver OnLoadingScreenClosed).
		void FinishCatchAnimation();

		// Consultado por Events::EquipGuard: si true, no debe deshacer el
		// último equipado del jugador aunque el estado no sea kInHand -- ver
		// EquipGestureWeapon.
		[[nodiscard]] bool IsEquipGuardSuppressed() const noexcept { return suppressEquipGuard; }

	private:
		WeaponManager() = default;
		~WeaponManager() = default;

		// Único punto por el que weaponState.SetState debe pasar (sustituye
		// a la llamada directa en las 11 transiciones reales) -- además del
		// cambio de estado en sí, enciende/apaga el VFX de movimiento (ver
		// 8.- ANIMATION/WeaponVFX.h) comparando a qué debe engancharse el
		// VFX antes y después (arma real en mano durante kAiming/kThrowing,
		// réplica en vuelo durante kThrown/kCalling/kReturning, nada en
		// kInHand/kStuck) -- solo reenganchar si ese objetivo cambia de
		// verdad, para no cortar y volver a arrancar el efecto en
		// transiciones entre dos estados que comparten el mismo objetivo
		// (p. ej. kAiming->kThrowing, ambos sobre el arma real). Excepción:
		// la transición kThrowing->kThrown NO arranca aquí el VFX sobre la
		// réplica -- en ese instante todavía no existe (Throw::LaunchWeapon
		// la crea de forma asíncrona, ver ThrowWeapon) -- se arranca a mano
		// en el callback onSpawned, en cuanto el handle real está listo.
		void TransitionState(State a_newState);

		// Fija como arma activa la que hay en la mano derecha y pasa a
		// "apuntando".
		void BeginAiming();

		// Pasa a "lanzando": activa la graph variable que gatea el submod de
		// OAR (Animation::SetThrowTrigger) y dispara el evento vanilla que
		// reproduce Throw.hkx en su lugar (Constants::kLightAttackAnimationEvent) --
		// el lanzamiento físico real no ocurre aquí, ver
		// OnThrowReleaseAnimationEvent. Arranca también la red de seguridad
		// por tiempo (Constants::kThrowReleaseFallbackWindow) por si la
		// anotación nunca llega.
		void BeginThrowAnimation();

		// Desequipa el arma activa (queda oculta y el jugador pasa a
		// combate desarmado), pasa a estado "lanzada" y arranca
		// Throw::LaunchWeapon para que la réplica visual vuele de verdad.
		// Llamado desde OnThrowReleaseAnimationEvent, nunca directamente
		// desde OnAimButtonUp -- ver BeginThrowAnimation.
		void ThrowWeapon();

		// Pasa a "llamando": escribe directamente
		// Constants::kRightHandTypeGraphVariable (graph variable vanilla,
		// Int) al valor de "arma de una mano" (Constants::kRightHandTypeOneHanded),
		// sin pasar por RE::ActorEquipManager en absoluto -- experimento que
		// sustituye al arma señuelo (EquipGestureWeapon, ver más abajo,
		// rechazado por el usuario: ~500ms de espera visible con el arma
		// real en pose de cuerpo a cuerpo mientras tanto, ver CHANGELOG
		// v1.10.11 a v1.10.15). Si el grafo respeta el valor, el cambio de
		// rama de combate es instantáneo y el arma real nunca se toca (nunca
		// visible, nada que ocultar). Activa la graph variable que gatea el
		// submod de OAR de Llamada (Animation::SetCallTrigger) y dispara el
		// mismo evento vanilla que Lanzar
		// (Constants::kLightAttackAnimationEvent) para que reproduzca
		// Call.hkx en su lugar -- el regreso físico real no ocurre aquí, ver
		// OnCallReleaseAnimationEvent. Arranca también la red de seguridad
		// por tiempo (Constants::kCallReleaseFallbackWindow) por si el
		// evento nunca llega. Recuerda si el arma venía de State::kStuck
		// (wasStuckBeforeCalling) para pasarlo a BeginReturn más tarde -- el
		// estado ya no es kStuck en ese momento (es kCalling), así que
		// BeginReturn no puede recalcularlo por su cuenta.
		void BeginCallAnimation();

		// Sin usar desde v1.10.16 (ver BeginCallAnimation) -- se mantienen
		// definidas como alternativa de reserva por si el experimento de
		// iRightHandType no funciona en el juego, para no tener que
		// reconstruirlas desde cero. Equipa de nuevo el arma real (la misma
		// que ya trackea weaponState, no una nueva) -- primer paso del truco
		// de "arma señuelo" para que el gesto de Llamada (y, más adelante,
		// Atrape) dispare attackStart estando "armado" a nivel de animation
		// graph, reutilizando la rama de combate a una mano (fiable, sin
		// variantes direccionales que cubrir ni deslizamiento, ver
		// CHANGELOG) en vez de la de cuerpo a cuerpo (desarmado, con ambos
		// problemas) -- rechazado por el usuario por la espera visible que
		// requiere (ver BeginCallAnimation). Activa suppressEquipGuard
		// mientras dura el equipado sin cola, para que Events::EquipGuard no
		// lo deshaga (ve el estado como != kInHand, igual que cualquier otro
		// equipado ajeno). Suprime también la animación de equipar
		// (SkipEquipAnimation, mismo truco que ReequipAndReset).
		void EquipGestureWeapon();

		// Inverso de EquipGestureWeapon: desequipa el arma señuelo (vuelve a
		// desarmado genuino) antes de que empiece el regreso físico real
		// (BeginReturn) -- el jugador sigue pudiendo golpear a puños durante
		// el vuelo de vuelta, igual que antes de este cambio.
		void UnequipGestureWeapon();

		// Regreso animado (5.- RETURN, puntos 7-8): cancela el bucle de
		// tick en marcha (vuelo, o seguimiento de un actor clavado),
		// libera al objetivo si lo había, y arranca Return::BeginReturn
		// sobre la réplica ya existente. Cae a RecallWeapon (recuperación
		// instantánea) si no hay jugador o réplica válida de la que
		// partir. a_wasStuck (punto 11): si el arma estaba clavada
		// (superficie o actor) en el instante de pulsar recuperar -- lo pasa
		// el llamante en vez de recalcularlo aquí, porque desde
		// OnCallReleaseAnimationEvent el estado actual ya es kCalling, no
		// kStuck.
		void BeginReturn(bool a_wasStuck);

		// Gesto visual de Atrape -- llamado desde el callback onApproaching
		// de Return::BeginReturn, sincronizado en vivo con el vuelo físico
		// real (medido tick a tick, no un temporizador precalculado de
		// antemano -- a petición del usuario, 2026-08-03: la
		// sincronización animación/física no es negociable). Se dispara
		// cuando de verdad quedan Constants::kCatchAnimationLeadTime
		// segundos (0.5s, medido por el usuario: duración total de
		// Catch.hkx menos el fotograma de su propia anotación) para la
		// llegada real, y solo si ya han pasado
		// Constants::kMinCatchAnimationDelay segundos reales desde que
		// empezó el regreso (0.5s, medido: lo que sigue reproduciéndose
		// Call.hkx tras su propia anotación -- disparar esto antes
		// confundía al grafo de forma no determinista, dos "attackStart"
		// vanilla demasiado seguidos). Si la distancia es tan corta que la
		// física natural no dejaría margen para ninguna de las dos cosas,
		// Return::BeginReturnMovement ralentiza el propio vuelo (nunca lo
		// acelera) en vez de desacoplar animación y física. A diferencia de
		// BeginThrowAnimation/BeginCallAnimation,
		// NO toca weaponState en absoluto -- el ciclo principal del arma
		// sigue en kReturning mientras dura este gesto, y solo pasa a
		// kInHand cuando OnCatchReleaseAnimationEvent llama a
		// ReequipAndReset (gatillado por la anotación real de Catch.hkx, no
		// por la llegada física en sí). Esto es puramente decorativo,
		// trackeado aparte con catchAnimationActive. Mismo mecanismo que
		// BeginCallAnimation por lo demás: escribe
		// Constants::kRightHandTypeGraphVariable a
		// Constants::kRightHandTypeOneHanded, activa Animation::SetCatchTrigger
		// y dispara Constants::kLightAttackAnimationEvent para que el submod
		// de OAR de Atrape reproduzca Catch.hkx. Arranca también la red de
		// seguridad por tiempo (Constants::kCatchReleaseFallbackWindow) por
		// si la anotación de liberación nunca llega.
		void BeginCatchAnimation();

		// Recuperación instantánea: destruye la réplica (si la hay, ver
		// Physics::DestroyReplica) y reequipa el arma de inmediato, sin
		// trayectoria de vuelta. Se mantiene como red de seguridad (cierre
		// de pantalla de carga a mitad de un regreso ya en marcha, o sin
		// jugador/réplica del que partir en BeginReturn) — el ciclo normal
		// de recuperación pasa por BeginReturn.
		void RecallWeapon();

		// Paso final común a la recuperación instantánea y a la llegada
		// del regreso animado: destruye la réplica, cancela cualquier
		// bucle de tick activo y reequipa el arma real (sin animación de
		// equipar/desenvainar, vía el mod externo SkipEquipAnimation, ver
		// CLAUDE.md). Sin transición de captura intermedia (probada en el
		// plan Kratos y retirada: con SkipEquipAnimation el reequipado ya
		// es limpio e instantáneo por sí solo, y el clon de la transición
		// se veía en una pose sin calibrar durante su ventana, más
		// perceptible aún al ser el reequipado real tan rápido).
		void ReequipAndReset();

		WeaponState weaponState;

		// Capturado en BeginCallAnimation antes de pasar a State::kCalling
		// (que ya no es kStuck), para que OnCallReleaseAnimationEvent pueda
		// pasárselo a BeginReturn más tarde -- ver comentario de BeginReturn.
		bool wasStuckBeforeCalling{ false };

		// Activado por EquipGestureWeapon mientras dura el equipado del
		// señuelo -- ver IsEquipGuardSuppressed.
		bool suppressEquipGuard{ false };

		// True mientras dura el gesto visual de Atrape (desde
		// BeginCatchAnimation hasta FinishCatchAnimation, ver esa función --
		// ya no hasta OnCatchReleaseAnimationEvent, desde que la limpieza
		// del grafo se difiere Constants::kCatchAnimationTailDuration, ver
		// CLAUDE.md 2026-08-08) -- deliberadamente independiente de
		// weaponState.GetState(), que puede haber vuelto a kInHand mucho
		// antes (el reequipado real no espera a este gesto, ver
		// BeginCatchAnimation/OnCatchReleaseAnimationEvent). Comprobado
		// también en OnLoadingScreenClosed/ResetToInHand para no dejar los
		// flags del grafo (CatchTrigger/AnimationDriven/movimiento
		// bloqueado) encendidos para siempre si una pantalla de carga o una
		// partida nueva interrumpe el gesto a mitad -- incluido ahora el
		// margen de espera de FinishCatchAnimation, no solo el clip en sí.
		bool catchAnimationActive{ false };

		// True desde que PerformCatchReequip dispara el reequipado real
		// (ReequipAndReset) hasta que FinishCatchAnimation limpia el resto
		// del gesto -- evita que la red de seguridad por tiempo
		// (Constants::kCatchReleaseFallbackWindow) repita el reequipado si
		// llega tarde, ya con catchAnimationActive todavía en true durante
		// el margen de espera de la cola del clip. Reseteado junto con
		// catchAnimationActive (BeginCatchAnimation, FinishCatchAnimation,
		// OnLoadingScreenClosed, ResetToInHand).
		bool catchReequipDone{ false };

		// True una vez que Return::ReturnCallbacks::onArrived confirma que
		// la réplica ha llegado de verdad, físicamente, a la mano (ver
		// OnPhysicalArrival) -- necesario porque la anotación de Catch.hkx
		// (OnCatchReleaseAnimationEvent, temporización fija) se calcula
		// sobre una predicción que puede quedarse corta en regresos largos
		// (bug reportado por el usuario, 2026-08-08, confirmado con logs
		// reales: la anotación llegaba antes que la propia llegada física
		// con la frecuencia suficiente para que el sonido de atrape casi
		// nunca sonara). Reseteado a false al arrancar cada regreso
		// (WeaponManager::BeginReturn).
		bool catchPhysicallyArrived{ false };

		// True si OnCatchReleaseAnimationEvent quiso reequipar (la
		// anotación real, o su red de seguridad) mientras
		// catchPhysicallyArrived todavía era false -- OnPhysicalArrival
		// completa el reequipado diferido (PerformCatchReequip) en cuanto
		// se confirma la llegada, en vez de perderlo. Reseteado a false al
		// arrancar cada regreso (WeaponManager::BeginReturn) y al
		// completarse (OnPhysicalArrival).
		bool catchReequipPending{ false };

		// Mismo papel que catchAnimationActive pero para Llamada -- true
		// desde BeginCallAnimation hasta FinishCallAnimation (ver esa
		// función y Constants::kCallAnimationTailDuration). No hace falta
		// un equivalente a catchReequipDone aquí: la parte "inmediata" de
		// OnCallReleaseAnimationEvent (BeginReturn) ya es idempotente por
		// su cuenta -- se guarda tras el cambio de weaponState a
		// State::kCalling, que BeginReturn deja atrás de inmediato.
		bool callAnimationActive{ false };
	};
}
