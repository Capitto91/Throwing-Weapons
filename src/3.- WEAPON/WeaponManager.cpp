// Implementación del ciclo de vida del arma.
// Coordina la transición entre arma en mano, arma lanzada y arma recuperada.

#include "3.- WEAPON/WeaponManager.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/SoundResolver.h"
#include "2.- INPUT/InputManager.h"
#include "4.- THROW/ThrowManager.h"
#include "5.- RETURN/ReturnManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponVFX.h"

#include <thread>

namespace Weapon
{
	namespace
	{
		// Ver el comentario de TransitionState en el header.
		enum class VfxTarget
		{
			kNone,
			kRealWeapon,
			kReplica
		};

		VfxTarget GetVfxTargetForState(State a_state)
		{
			switch (a_state) {
			case State::kAiming:
			case State::kThrowing:
				return VfxTarget::kRealWeapon;
			case State::kThrown:
			case State::kCalling:
			case State::kReturning:
				return VfxTarget::kReplica;
			default:
				return VfxTarget::kNone;  // kInHand, kStuck
			}
		}
	}

	WeaponManager* WeaponManager::GetSingleton()
	{
		static WeaponManager singleton;
		return &singleton;
	}

	void WeaponManager::TransitionState(State a_newState)
	{
		const auto oldTarget = GetVfxTargetForState(weaponState.GetState());
		weaponState.SetState(a_newState);
		const auto newTarget = GetVfxTargetForState(a_newState);

		logs::info("WeaponManager::TransitionState: -> {} (oldTarget={}, newTarget={}).",
			static_cast<int>(a_newState), static_cast<int>(oldTarget), static_cast<int>(newTarget));

		if (newTarget == oldTarget) {
			return;
		}

		Animation::StopMovementVFX();

		switch (newTarget) {
		case VfxTarget::kRealWeapon:
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Animation::StartMovementVFXOnActor(*player);
			}
			break;
		case VfxTarget::kReplica:
			// Excepción kThrowing->kThrown, ver el comentario del header:
			// si el handle todavía no es válido, la réplica no existe
			// todavía -- ThrowWeapon lo arranca a mano en cuanto
			// onSpawned confirme un handle real.
			if (auto handle = weaponState.GetActiveReplicaHandle(); handle.get()) {
				Animation::StartMovementVFXOnReplica(handle);
			}
			break;
		case VfxTarget::kNone:
			break;
		}
	}

	void WeaponManager::OnAimButtonDown()
	{
		switch (weaponState.GetState()) {
		case State::kInHand:
			{
				// Mismo patrón que el ataque cuerpo a cuerpo vanilla con el
				// arma envainada: la primera pulsación solo desenvaina, no
				// empieza a apuntar -- hace falta una segunda pulsación ya con
				// el arma desenvainada. Sin esto, el arma salía disparada sin
				// llegar a reproducir Throw.hkx: el propio desenvainado (que
				// tiene su propia animación, con tiempo real) todavía no había
				// terminado en el instante en que se disparaba attackStart.
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (player && !player->AsActorState()->IsWeaponDrawn()) {
					player->DrawWeaponMagicHands(true);
					break;
				}
				BeginAiming();
				break;
			}
		case State::kAiming:
			// Solo se puede recibir una pulsación nueva estando ya
			// "apuntando" si nos perdimos el botón de soltar anterior (p.ej.
			// una pantalla de carga a mitad de la pulsación): no hay nada
			// que deshacer todavía, así que reiniciamos el ciclo con la
			// pulsación actual en vez de quedarnos atascados.
			TransitionState(State::kInHand);
			BeginAiming();
			break;
		default:
			break;
		}
	}

	void WeaponManager::OnAimButtonUp()
	{
		switch (weaponState.GetState()) {
		case State::kAiming:
			BeginThrowAnimation();
			break;
		case State::kThrown:
		case State::kStuck:
			// Disparado al soltar, no al pulsar -- mismo motivo que Lanzar:
			// disparar attackStart mientras el botón todavía está pulsado
			// escalaba a un power attack vanilla real (confirmado en el
			// juego con el Animation Event Log de OAR: PowerAttack_Start_end
			// en vez de la secuencia del ataque ligero, incluso con el
			// personaje quieto -- no era el mismo bug de movimiento ya
			// resuelto para Lanzar en v1.9.16). Al soltar, el botón ya no
			// está pulsado en el instante exacto de NotifyAnimationGraph, así
			// que no hay ambigüedad que resolver.
			BeginCallAnimation();
			break;
		default:
			break;
		}
	}

	void WeaponManager::ResetToInHand()
	{
		// No se destruye ninguna réplica aquí (a diferencia de
		// RecallWeapon): al cargar/empezar partida no hay ninguna réplica
		// real que limpiar en el mundo todavía, solo se olvida el handle
		// por si quedaba uno obsoleto de una sesión de juego anterior
		// dentro del mismo proceso.
		weaponState.SetActiveWeapon(nullptr);
		weaponState.SetActiveReplicaHandle({});
		weaponState.SetStuckActorHandle({});
		weaponState.SetActiveTickToken({});
		TransitionState(State::kInHand);

		// Capturado antes de limpiarlo más abajo -- solo si nuestro propio
		// código había escrito iRightHandType a mano (BeginCallAnimation)
		// hace falta revertirlo aquí. Escribirlo siempre a 0 (como se hacía
		// antes) rompía el caso normal de morir/cargar con el arma
		// simplemente equipada en la mano (ningún ciclo en marcha): el
		// personaje aparecía en pose de cuerpo a cuerpo con la animación
		// vanilla de golpeo hasta forzar al grafo a releer el valor
		// (bug reportado por el usuario, 2026-08-08) -- mismo mecanismo ya
		// documentado en PerformCatchReequip/OnLoadingScreenClosed: esta
		// graph variable no debe tocarse salvo que de verdad hayamos sido
		// nosotros quienes la desincronizaron del equipado real.
		const bool wasCallAnimationActive = callAnimationActive;
		catchAnimationActive = false;
		catchReequipDone = false;
		catchPhysicallyArrived = false;
		catchReequipPending = false;
		callAnimationActive = false;

		// Por si el estado anterior era kAiming (sin esto, el offset de zoom
		// se quedaría aplicado para siempre) -- desactivar un zoom que ya
		// estaba desactivado es un no-op inofensivo, mismo criterio que
		// Input::SetMovementLocked(false) más abajo.
		Animation::SetAimZoom(false);

		// Por si el estado anterior era kThrowing (partida guardada/cargada
		// a mitad de esa ventana, dentro de la misma sesión del proceso):
		// sin esto, el movimiento/la graph variable se quedarían activos
		// para siempre. Llamar aquí sin comprobar el estado previo es
		// seguro -- desactivar algo que ya estaba desactivado es un no-op
		// inofensivo (ver Input::SetMovementLocked/Animation::SetAnimationDriven).
		Input::SetMovementLocked(false);
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetAnimationDriven(*player, false);
			Animation::SetThrowTrigger(*player, false);
			Animation::SetCallTrigger(*player, false);
			Animation::SetCatchTrigger(*player, false);
			if (wasCallAnimationActive) {
				player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, 0);
			}
		}
	}

	WeaponManager::SaveCycleData WeaponManager::CaptureSaveData() const
	{
		SaveCycleData data;
		data.cycleActive = weaponState.GetState() == State::kThrown ||
		                   weaponState.GetState() == State::kStuck ||
		                   weaponState.GetState() == State::kCalling ||
		                   weaponState.GetState() == State::kReturning;

		if (!data.cycleActive) {
			return data;
		}

		if (auto* weapon = weaponState.GetActiveWeapon()) {
			data.weaponFormID = weapon->GetFormID();
		}
		if (auto replica = weaponState.GetActiveReplicaHandle().get()) {
			data.replicaFormID = replica->GetFormID();
		}
		if (auto actor = weaponState.GetStuckActorHandle().get()) {
			data.stuckActorFormID = actor->GetFormID();
		}

		return data;
	}

	void WeaponManager::RecoverOrReset(const SaveCycleData& a_data)
	{
		if (!a_data.cycleActive) {
			ResetToInHand();
			return;
		}

		logs::info("WeaponManager::RecoverOrReset: la partida se guardó a medias de un ciclo, recuperando el arma real.");

		if (a_data.stuckActorFormID) {
			auto* actorForm = RE::TESForm::LookupByID(a_data.stuckActorFormID);
			if (auto* actor = actorForm ? actorForm->As<RE::Actor>() : nullptr) {
				Combat::EndEmbeddedEffect(actor);
			}
		}

		if (a_data.replicaFormID) {
			auto* replicaForm = RE::TESForm::LookupByID(a_data.replicaFormID);
			if (auto* replicaRefr = replicaForm ? replicaForm->As<RE::TESObjectREFR>() : nullptr) {
				Physics::DestroyReplica(RE::ObjectRefHandle(replicaRefr));
			}
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weaponForm = a_data.weaponFormID ? RE::TESForm::LookupByID(a_data.weaponFormID) : nullptr;
		auto* weapon = weaponForm ? weaponForm->As<RE::TESBoundObject>() : nullptr;

		if (player && weapon) {
			// Mismo motivo que ReequipAndReset: llamado síncrono aquí falla
			// en silencio (comprobado en la iteración anterior).
			SKSE::GetTaskInterface()->AddTask([player, weapon]() {
				RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
			});
		} else {
			logs::warn("WeaponManager::RecoverOrReset: el arma guardada ya no se resuelve, no se reequipa nada.");
		}

		ResetToInHand();
	}

	void WeaponManager::OnLoadingScreenClosed()
	{
		switch (weaponState.GetState()) {
		case State::kThrown:
		case State::kStuck:
		case State::kReturning:
			// Regreso ya en marcha a mitad de trayecto (p. ej. viaje
			// rápido mientras el arma volvía): se aborta con la
			// recuperación instantánea en vez de dejarlo continuar sobre
			// una réplica que puede haber quedado en una celda distinta.
			RecallWeapon();
			break;
		case State::kAiming:
			// El arma sigue en la mano (el desequipar solo pasa al soltar);
			// solo reordenamos el estado por si la pulsación de soltar se
			// perdió durante la carga.
			TransitionState(State::kInHand);
			Animation::SetAimZoom(false);
			break;
		case State::kThrowing:
			// El arma tampoco ha llegado a desequiparse todavía en este
			// estado (eso solo pasa en ThrowWeapon, al recibir la anotación
			// de liberación) -- mismo caso que kAiming, pero además hay que
			// apagar la graph variable, o el submod de OAR se quedaría
			// sustituyendo el ataque ligero indefinidamente, y desbloquear
			// el movimiento (ver BeginThrowAnimation) o se quedaría
			// bloqueado para siempre.
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Animation::SetThrowTrigger(*player, false);
				Animation::SetAnimationDriven(*player, false);
			}
			Input::SetMovementLocked(false);
			TransitionState(State::kInHand);
			break;
		case State::kCalling:
			// A diferencia de kThrowing, el arma sigue fuera de la mano
			// aquí (mismo estado físico que kThrown/kStuck, solo con el
			// gesto de Llamada reproduciéndose encima) -- RecallWeapon,
			// no un simple cambio de estado, o la réplica se quedaría
			// huérfana en el mundo.
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Animation::SetCallTrigger(*player, false);
				Animation::SetAnimationDriven(*player, false);
			}
			Input::SetMovementLocked(false);
			RecallWeapon();
			break;
		default:
			break;
		}

		// Aparte del switch anterior: mientras dura el gesto visual de
		// Atrape, weaponState sigue en kReturning (el reequipado real está
		// gatillado por la anotación de Catch.hkx, no por la llegada
		// física -- ver OnCatchReleaseAnimationEvent), así que el caso
		// kReturning de arriba ya llama a RecallWeapon() y reequipa el
		// arma real de verdad -- pero no toca los flags propios del gesto
		// (CatchTrigger/AnimationDriven/catchAnimationActive/bloqueo de
		// movimiento), que sin esto se quedarían encendidos para siempre.
		if (catchAnimationActive) {
			catchAnimationActive = false;
			catchReequipDone = false;
			catchPhysicallyArrived = false;
			catchReequipPending = false;
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Animation::SetCatchTrigger(*player, false);
				Animation::SetAnimationDriven(*player, false);
				// iRightHandType NO se toca aquí -- mismo motivo que
				// OnCatchReleaseAnimationEvent: RecallWeapon ya reequipó el
				// arma real de verdad justo arriba, y resetear esta graph
				// variable después de un reequipado real deja al personaje
				// en pose de cuerpo a cuerpo hasta la siguiente acción que
				// fuerce al grafo a releerla (bug reportado por el
				// usuario, 2026-08-04).
			}
			Input::SetMovementLocked(false);
		}

		// Mismo motivo que el bloque de catchAnimationActive de arriba, para
		// Llamada (2026-08-08, ver CLAUDE.md/Constants::kCallAnimationTailDuration):
		// desde que FinishCallAnimation se difiere, hay una ventana (tras la
		// anotación de liberación de Call.hkx, antes de que se cumpla ese
		// margen) en la que weaponState ya ha salido de kCalling (a
		// kReturning, vía BeginReturn) pero CallTrigger/AnimationDriven/el
		// bloqueo de movimiento siguen encendidos -- el caso kReturning del
		// switch de arriba ya llama a RecallWeapon() para esa ventana, pero
		// no toca estos flags. Redundante-pero-inofensivo si la interrupción
		// llegó en cambio mientras weaponState todavía era kCalling (el
		// case de ahí arriba ya los había limpiado) -- desactivar algo que
		// ya estaba desactivado no hace nada.
		if (callAnimationActive) {
			callAnimationActive = false;
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Animation::SetCallTrigger(*player, false);
				Animation::SetAnimationDriven(*player, false);
				player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, 0);
			}
			Input::SetMovementLocked(false);
		}
	}

	void WeaponManager::BeginAiming()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		// Decisión no cubierta por Mecanica del arma.txt (no menciona el
		// sigilo): no se puede empezar a apuntar/lanzar estando agachado --
		// a petición del usuario. Comprobado aquí (no en OnAimButtonDown)
		// para que aplique igual desde la primera pulsación que desde la
		// resincronización de kAiming.
		if (player->AsActorState()->IsSneaking()) {
			return;
		}

		auto* weapon = player->GetEquippedObject(false);
		auto* boundWeapon = weapon ? weapon->As<RE::TESBoundObject>() : nullptr;

		if (!boundWeapon) {
			return;
		}

		weaponState.SetActiveWeapon(boundWeapon);
		TransitionState(State::kAiming);

		// Zoom de cámara mientras dura el apuntado -- puro polish, no cubierto
		// por Mecanica del arma.txt (ver Constants::kAimZoomThirdPersonOffset).
		// Revertido en BeginThrowAnimation (salida normal) y en
		// OnLoadingScreenClosed/ResetToInHand (salida por pantalla de carga o
		// reinicio).
		Animation::SetAimZoom(true);

		// El arma ya se desenvainó antes de llegar aquí -- ver
		// OnAimButtonDown, que ahora exige una pulsación previa dedicada
		// solo a desenvainar (igual que el ataque cuerpo a cuerpo vanilla
		// con el arma envainada) antes de empezar a apuntar de verdad. No
		// se llama a DrawWeaponMagicHands aquí para no arriesgar el blip
		// visual de llamarla con el arma ya desenvainada (no hay garantía
		// de que sea idempotente, sin cuerpo documentado en
		// commonlibsse-ng).
	}

	void WeaponManager::BeginThrowAnimation()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		TransitionState(State::kThrowing);

		// Fin del zoom de apuntado (ver BeginAiming) -- el gesto de Lanzar ya
		// no es "apuntando".
		Animation::SetAimZoom(false);

		// Bloquea el movimiento mientras dura la animación de Lanzar:
		// atacar mientras te mueves (incluso si empiezas a moverte a mitad
		// del clip, no solo al dispararlo) escala automáticamente a un
		// power attack direccional vanilla (1HM_AttackPowerFwd/Bwd/Left/
		// Right) -- un clip que el submod de OAR no sustituye, así que se ve
		// y aplica daño como un ataque real en vez de reproducir Throw.hkx.
		// Comprobado en el juego con el Animation Event Log de OAR (ver
		// _reference/PLAN-OAR.md): moverse antes de la anotación de
		// liberación, "Pie.MjolnirThrow" nunca llega. Desbloqueado en
		// OnThrowReleaseAnimationEvent (cubre tanto la anotación real como
		// la red de seguridad, ver más abajo) y en los caminos de
		// recuperación (OnLoadingScreenClosed/ResetToInHand).
		Input::SetMovementLocked(true);

		// Prueba (ver Constants::kAnimationDrivenGraphVariable): variable
		// vanilla, no propia -- a ver si evita que el motor decida "power
		// attack direccional" cuando el jugador ya llevaba movimiento al
		// soltar el botón, que Input::SetMovementLocked por sí solo no
		// evita (bloquea input *nuevo*, no el momentum ya acumulado).
		Animation::SetAnimationDriven(*player, true);

		// Fase 3 del plan OAR (_reference/PLAN-OAR.md): el Global gatea el
		// submod de OAR que sustituye Constants::kLightAttackAnimationEvent
		// (un evento vanilla ya existente, ninguno nuevo) por Throw.hkx.
		Animation::SetThrowTrigger(*player, true);
		const bool notifyOk = player->NotifyAnimationGraph(Constants::kLightAttackAnimationEvent);
		logs::info("WeaponManager::BeginThrowAnimation: '{}' disparado, NotifyAnimationGraph()={}.",
			Constants::kLightAttackAnimationEvent, notifyOk);

		// Red de seguridad: el lanzamiento físico debe ocurrir siempre, tenga
		// o no confirmación de la anotación real (decisión del usuario,
		// 2026-07-29) -- lo que se sigue depurando es solo la sincronía
		// visual con la animación, nunca a costa de dejar el arma inutilizable
		// si esa sincronía falla. Mismo patrón hilo-que-duerme-y-reencola del
		// resto del proyecto; OnThrowReleaseAnimationEvent ya comprueba el
		// estado, así que llamarla de más aquí si la anotación real llegó
		// antes es inofensivo (no-op).
		std::thread([this]() {
			std::this_thread::sleep_for(Constants::kThrowReleaseFallbackWindow);
			SKSE::GetTaskInterface()->AddTask([this]() {
				if (weaponState.GetState() == State::kThrowing) {
					logs::info("WeaponManager: red de seguridad disparada -- la anotación de liberación nunca llegó.");
				}
				OnThrowReleaseAnimationEvent();
			});
		}).detach();
	}

	void WeaponManager::OnThrowReleaseAnimationEvent()
	{
		if (weaponState.GetState() != State::kThrowing) {
			return;
		}

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetThrowTrigger(*player, false);
		}

		// Input::SetMovementLocked/Animation::SetAnimationDriven NO se
		// desactivan aquí todavía -- desactivarlos en este mismo instante,
		// con el jugador todavía moviéndose, producía un tirón visual
		// brusco (comprobado en el juego con el Animation Event Log: el
		// personaje pasaba de golpe de la pose animada al reasumir el
		// control de movimiento real a mitad de zancada). Se desactivan
		// junto con el desequipado real, ver ThrowWeapon.
		ThrowWeapon();
	}

	void WeaponManager::BeginCallAnimation()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		// Capturado antes de cambiar de estado -- BeginReturn ya no puede
		// leerlo de weaponState.GetState() una vez aquí es kCalling, no
		// kStuck.
		wasStuckBeforeCalling = weaponState.GetState() == State::kStuck;
		TransitionState(State::kCalling);
		callAnimationActive = true;

		// Mismo motivo que en BeginThrowAnimation: evitar que moverse
		// durante el gesto de Llamada escale a un power attack direccional
		// vanilla en vez de reproducir Call.hkx.
		Input::SetMovementLocked(true);
		Animation::SetAnimationDriven(*player, true);

		// Experimento (sustituye al arma señuelo -- EquipGestureWeapon/
		// UnequipGestureWeapon quedan definidas más abajo sin usar, de
		// reserva si esto no funciona, ver CHANGELOG v1.10.15): escribe
		// directamente iRightHandType al valor de "arma de una mano", sin
		// pasar por RE::ActorEquipManager en absoluto -- si el grafo respeta
		// el valor, el cambio de rama de combate es instantáneo y no hay
		// nada que equipar ni ocultar (el arma real nunca se toca).
		const std::int32_t previousRightHandType = [player]() {
			std::int32_t value = 0;
			player->GetGraphVariableInt(Constants::kRightHandTypeGraphVariable, value);
			return value;
		}();
		player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, Constants::kRightHandTypeOneHanded);

		Animation::SetCallTrigger(*player, true);
		const bool notifyOk = player->NotifyAnimationGraph(Constants::kLightAttackAnimationEvent);

		std::int32_t readBackInt = -1;
		player->GetGraphVariableInt(Constants::kRightHandTypeGraphVariable, readBackInt);
		logs::info("WeaponManager::BeginCallAnimation: '{}' puesto a {} (era {}), releído como {} -- '{}' disparado, NotifyAnimationGraph()={}.",
			Constants::kRightHandTypeGraphVariable, Constants::kRightHandTypeOneHanded, previousRightHandType, readBackInt,
			Constants::kLightAttackAnimationEvent, notifyOk);

		// Red de seguridad: el regreso físico debe empezar siempre, tenga o
		// no confirmación de la anotación real -- mismo criterio que
		// BeginThrowAnimation.
		std::thread([this]() {
			std::this_thread::sleep_for(Constants::kCallReleaseFallbackWindow);
			SKSE::GetTaskInterface()->AddTask([this]() {
				if (weaponState.GetState() == State::kCalling) {
					logs::info("WeaponManager: red de seguridad de Llamada disparada -- la anotación de liberación nunca llegó.");
				}
				OnCallReleaseAnimationEvent();
			});
		}).detach();
	}

	void WeaponManager::OnCallReleaseAnimationEvent()
	{
		if (weaponState.GetState() != State::kCalling) {
			return;
		}

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			// El sonido del chasquido ya no depende de un SoundPlay vanilla
			// (descartado, ver CHANGELOG.md) -- se dispara aquí mismo, en el
			// mismo instante que el regreso físico real.
			Audio::PlayReliableOneShot(player->GetPosition(), Constants::kCallReleaseSoundLocalFormID, Constants::kCallReleaseSoundEditorID);
		}

		// Debe ocurrir exactamente en este instante, sincronizado con la
		// anotación real -- el resto del gesto (desatascar el grafo) se
		// difiere, ver FinishCallAnimation.
		BeginReturn(wasStuckBeforeCalling);

		std::thread([this]() {
			std::this_thread::sleep_for(Constants::kCallAnimationTailDuration);
			SKSE::GetTaskInterface()->AddTask([this]() {
				FinishCallAnimation();
			});
		}).detach();
	}

	void WeaponManager::FinishCallAnimation()
	{
		if (!callAnimationActive) {
			// Ya limpiado por otra vía (p. ej. pantalla de carga a mitad de
			// este margen de espera, ver OnLoadingScreenClosed) -- no repetir.
			return;
		}
		callAnimationActive = false;

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetCallTrigger(*player, false);
			Animation::SetAnimationDriven(*player, false);

			// Vuelve iRightHandType a 0 (desarmado) -- el jugador nunca ha
			// dejado de estar genuinamente desarmado (el arma real sigue sin
			// equipar todo este rato), esto solo revierte el valor de la
			// graph variable que se puso a mano en BeginCallAnimation.
			player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, 0);

			// Ver Constants::kAttackStopAnimationEvent: confirmado en el
			// juego que desatasca el grafo de AttackRight_State (el submod de
			// OAR de Llamada ignora los triggers horneados en el clip
			// vainilla sustituido, ver ahí el porqué). Disparado aquí, tras
			// Constants::kCallAnimationTailDuration, en vez de en el mismo
			// instante que la anotación de liberación -- para no cortar la
			// cola visual de Call.hkx (bug reportado por el usuario,
			// 2026-08-08: "la animación de Llamada queda cortada").
			player->NotifyAnimationGraph(Constants::kAttackStopAnimationEvent);
		}
		Input::SetMovementLocked(false);
	}

	void WeaponManager::BeginCatchAnimation()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			// Sin jugador no hay grafo sobre el que escribir nada ni
			// animación que reproducir -- recuperación instantánea directa,
			// igual que en Lanzar/Llamada (ver BeginThrowAnimation).
			ReequipAndReset();
			return;
		}
		if (catchAnimationActive) {
			// No debería poder llamarse dos veces (onApproaching se dispara
			// una sola vez por regreso, ver Return::ApproachTrigger), pero
			// comprobarlo aquí evita re-disparar el trigger/red de
			// seguridad por error si algún día deja de serlo.
			return;
		}

		// A diferencia de Lanzar/Llamada, el ciclo principal del arma
		// (weaponState) no pasa por aquí ni se ve afectado: la réplica sigue
		// su propio bucle de tick en Return::BeginReturn (todavía en vuelo
		// en este instante -- esto se dispara con antelación, ver
		// Constants::kCatchAnimationLeadTime), indiferente a esto.
		// Trackeado aparte con catchAnimationActive en vez de
		// weaponState.GetState() porque el reequipado real (más abajo, en
		// OnCatchReleaseAnimationEvent) puede llegar antes o después de que
		// weaponState ya haya cambiado de estado por su cuenta.
		catchAnimationActive = true;
		catchReequipDone = false;

		// Mismo motivo que en BeginCallAnimation: evitar que moverse durante
		// el gesto de Atrape escale a un power attack direccional vanilla en
		// vez de reproducir Catch.hkx.
		Input::SetMovementLocked(true);
		Animation::SetAnimationDriven(*player, true);

		// Mismo mecanismo confirmado en Llamada (ver BeginCallAnimation):
		// escribe iRightHandType directamente, sin pasar por
		// RE::ActorEquipManager -- el arma real todavía no se ha reequipado
		// en este punto (eso lo hace OnCatchReleaseAnimationEvent, al
		// llegar la anotación PIE.ThorMjolnirCatch).
		const std::int32_t previousRightHandType = [player]() {
			std::int32_t value = 0;
			player->GetGraphVariableInt(Constants::kRightHandTypeGraphVariable, value);
			return value;
		}();
		player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, Constants::kRightHandTypeOneHanded);

		Animation::SetCatchTrigger(*player, true);
		const bool notifyOk = player->NotifyAnimationGraph(Constants::kLightAttackAnimationEvent);

		std::int32_t readBackInt = -1;
		player->GetGraphVariableInt(Constants::kRightHandTypeGraphVariable, readBackInt);
		logs::info("WeaponManager::BeginCatchAnimation: '{}' puesto a {} (era {}), releído como {} -- '{}' disparado, NotifyAnimationGraph()={}.",
			Constants::kRightHandTypeGraphVariable, Constants::kRightHandTypeOneHanded, previousRightHandType, readBackInt,
			Constants::kLightAttackAnimationEvent, notifyOk);

		// Red de seguridad: el reequipado real debe ocurrir siempre, tenga o
		// no confirmación de la anotación real -- mismo criterio que
		// BeginThrowAnimation/BeginCallAnimation. Constants::kCatchReleaseFallbackWindow
		// (1.5s) debe ser mayor que Constants::kCatchAnimationLeadTime (0,5s,
		// ver Constants.h para la medición sobre el propio clip) con margen
		// de sobra, o esta red de seguridad podría dispararse antes de que
		// la réplica llegue de verdad a la mano.
		std::thread([this]() {
			std::this_thread::sleep_for(Constants::kCatchReleaseFallbackWindow);
			SKSE::GetTaskInterface()->AddTask([this]() {
				if (catchAnimationActive) {
					logs::info("WeaponManager: red de seguridad de Atrape disparada -- la anotación de liberación nunca llegó.");
				}
				OnCatchReleaseAnimationEvent();
			});
		}).detach();
	}

	void WeaponManager::OnCatchReleaseAnimationEvent()
	{
		if (!catchAnimationActive || catchReequipDone) {
			return;
		}
		logs::info("WeaponManager::OnCatchReleaseAnimationEvent: anotación de liberación recibida.");

		// Cambio de criterio (2026-08-08, a petición del usuario, tras
		// confirmar con logs reales del juego): esta anotación tiene
		// temporización fija (ver Constants::kCatchAnimationLeadTime),
		// calculada sobre una predicción de Return::BeginReturnMovement de
		// cuándo va a llegar la réplica -- en regresos largos, donde el
		// jugador ha tenido más tiempo para moverse, esa predicción puede
		// quedarse corta y esta anotación llegar antes de que la réplica
		// haya llegado de verdad a la mano. Reequipar en ese caso cortaba
		// la animación a medias y cancelaba el bucle de tick de
		// Return::BeginReturnMovement antes de que este detectara la
		// llegada física por su cuenta -- por eso tampoco sonaba
		// Audio::CatchCue::PlayEnd (solo se dispara al detectarla). En vez
		// de seguir afinando esa predicción (ya van dos rondas), se
		// comprueba aquí la confirmación real de llegada física
		// (catchPhysicallyArrived, ver Return::ReturnCallbacks::onArrived/
		// OnPhysicalArrival) -- si todavía no ha llegado, se difiere el
		// reequipado hasta que sí (catchReequipPending), en vez de fiarse a
		// ciegas de esta temporización. En el caso normal (la réplica ya
		// ha llegado, o llega antes que esta anotación -- la mayoría de las
		// veces) no cambia nada, PerformCatchReequip se llama exactamente
		// igual de inmediato.
		if (!catchPhysicallyArrived) {
			logs::info("WeaponManager::OnCatchReleaseAnimationEvent: la réplica todavía no ha llegado de verdad -- reequipado diferido hasta que llegue.");
			catchReequipPending = true;
			return;
		}

		PerformCatchReequip();
	}

	void WeaponManager::OnPhysicalArrival()
	{
		catchPhysicallyArrived = true;
		if (catchReequipPending) {
			catchReequipPending = false;
			PerformCatchReequip();
		}
	}

	void WeaponManager::PerformCatchReequip()
	{
		catchReequipDone = true;

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			// Temblor de cámara al cerrar la mano sobre el arma -- ver
			// Constants::kCatchShakeStrength/kCatchShakeDuration. Debe
			// coincidir con el reequipado real de abajo (el instante en que
			// la mano se cierra de verdad en el clip), no con el final del
			// gesto -- epicentro en el propio jugador para que la
			// atenuación por distancia del motor no le reste fuerza a su
			// propia cámara.
			RE::ShakeCamera(Constants::kCatchShakeStrength, player->GetPosition(), Constants::kCatchShakeDuration);
		}

		// A diferencia de la llegada física en sí (BeginReturn, callback
		// onArrived, que deja la réplica quieta pero no reequipa nada): la
		// anotación PIE.ThorMjolnirCatch, ya horneada en Catch.hkx, marca el
		// instante exacto en que la mano se cierra sobre el arma en el
		// propio clip -- confiar en ella en vez de en el umbral de
		// distancia de la llegada física es lo que sincroniza de verdad el
		// reequipado visual con el gesto de la animación (salvo el caso
		// diferido de arriba, donde la llegada física real llegó tarde).
		//
		// iRightHandType NO se toca aquí (a diferencia de
		// OnCallReleaseAnimationEvent/FinishCallAnimation, que sí lo vuelve
		// a 0): la llamada real a RE::ActorEquipManager::EquipObject de
		// dentro de ReequipAndReset debe quedarse como dueña de esa graph
		// variable a partir de ahora. Bug reportado por el usuario
		// (2026-08-04): resetear a 0 aquí y reequipar de verdad justo
		// después dejaba al personaje en pose de cuerpo a cuerpo hasta la
		// siguiente acción (ataque, etc.) que forzara al grafo a releer el
		// valor -- el reequipado real no parece disparar por sí solo ese
		// refresco. Ya está en el valor correcto
		// (Constants::kRightHandTypeOneHanded, puesto en
		// BeginCatchAnimation) para lo que va a ser cierto de verdad en un
		// instante, así que tocarlo aquí sobra.
		ReequipAndReset();

		// Cambio de criterio (2026-08-08, a petición del usuario, ver
		// CLAUDE.md/Constants::kCatchAnimationTailDuration): el resto del
		// gesto (desatascar el grafo, soltar el bloqueo de movimiento) se
		// difiere en vez de hacerse aquí mismo -- Catch.hkx sigue teniendo
		// fotogramas propios sin reproducir después de esta anotación
		// (mide ~1s total, la anotación cae a los
		// Constants::kCatchAnimationLeadTime, 0,5s), y cortarlo con
		// attackStop en el mismo instante que el reequipado descartaba esa
		// segunda mitad del clip siempre, no solo a veces (reportado por el
		// usuario: "la animación de Atrape queda cortada"). Mismo patrón
		// que Throw::ThrowWeapon ya usa para su propio hueco
		// (Constants::kThrowReleaseVisualHoldDuration).
		std::thread([this]() {
			std::this_thread::sleep_for(Constants::kCatchAnimationTailDuration);
			SKSE::GetTaskInterface()->AddTask([this]() {
				FinishCatchAnimation();
			});
		}).detach();
	}

	void WeaponManager::FinishCatchAnimation()
	{
		if (!catchAnimationActive) {
			// Ya limpiado por otra vía (p. ej. pantalla de carga a mitad de
			// este margen de espera, ver OnLoadingScreenClosed) -- no repetir.
			return;
		}
		catchAnimationActive = false;
		catchReequipDone = false;
		catchPhysicallyArrived = false;
		catchReequipPending = false;

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetCatchTrigger(*player, false);
			Animation::SetAnimationDriven(*player, false);

			// Ver Constants::kAttackStopAnimationEvent -- mismo motivo que
			// OnCallReleaseAnimationEvent/FinishCallAnimation. Disparado aquí,
			// tras Constants::kCatchAnimationTailDuration, no en el instante
			// de la anotación de liberación (ver OnCatchReleaseAnimationEvent).
			player->NotifyAnimationGraph(Constants::kAttackStopAnimationEvent);
		}
		Input::SetMovementLocked(false);
	}

	void WeaponManager::EquipGestureWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();
		if (!player || !weapon) {
			return;
		}

		// Mientras dure el equipado (hasta que se apague más abajo),
		// Events::EquipGuard no debe deshacerlo -- ve el estado como
		// != kInHand, igual que cualquier otro equipado ajeno al ciclo.
		suppressEquipGuard = true;

		// Mismo truco que ReequipAndReset (SkipEquipAnimation, mod externo,
		// ver CLAUDE.md) para que este reequipado no dispare la animación de
		// desenvainado real -- aquí interesa aún más que en ReequipAndReset,
		// el gesto entero dura solo un par de segundos.
		player->SetGraphVariableBool("SkipEquipAnimation", true);
		RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
		logs::info("WeaponManager::EquipGestureWeapon: EquipObject llamado (arma señuelo, FormID 0x{:08X}), suppressEquipGuard/SkipEquipAnimation activos.", weapon->GetFormID());

		// No se oculta aquí todavía -- comprobado en el juego que
		// EquipObject no deja el equipado (ni la selección de rama de
		// combate) listo en este mismo instante: ocultar y disparar
		// attackStart justo aquí seguía reproduciendo el ataque desarmado
		// con el arma real visible. El llamante (BeginCallAnimation) espera
		// antes de ocultar y disparar el evento.

		// EquipObject no procesa el equipado de verdad de forma síncrona
		// (mismo motivo ya documentado en ReequipAndReset) -- apagar
		// SkipEquipAnimation/suppressEquipGuard en el mismo instante dejaría
		// pasar la animación real o el TESEquipEvent real sin suprimir. Se
		// desactivan aparte, tras Constants::kSkipEquipAnimationWindow,
		// mismo patrón hilo-que-duerme-y-reencola de todo el proyecto.
		std::thread([this, player]() {
			std::this_thread::sleep_for(Constants::kSkipEquipAnimationWindow);
			SKSE::GetTaskInterface()->AddTask([this, player]() {
				player->SetGraphVariableBool("SkipEquipAnimation", false);
				suppressEquipGuard = false;
				logs::info("WeaponManager::EquipGestureWeapon: ventana cumplida, SkipEquipAnimation/suppressEquipGuard desactivados.");
			});
		}).detach();
	}

	void WeaponManager::UnequipGestureWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();
		if (!player || !weapon) {
			return;
		}

		// No hace falta suprimir EquipGuard aquí -- solo reacciona a
		// TESEquipEvent con a_event->equipped == true, nunca a un
		// desequipado.
		player->SetGraphVariableBool("SkipEquipAnimation", true);
		RE::ActorEquipManager::GetSingleton()->UnequipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
		logs::info("WeaponManager::UnequipGestureWeapon: UnequipObject llamado (arma señuelo), vuelta a desarmado genuino.");

		std::thread([player]() {
			std::this_thread::sleep_for(Constants::kSkipEquipAnimationWindow);
			SKSE::GetTaskInterface()->AddTask([player]() {
				player->SetGraphVariableBool("SkipEquipAnimation", false);
			});
		}).detach();
	}

	void WeaponManager::ThrowWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();

		if (player && weapon) {
			// El arma real se oculta primero, no se desequipa todavía:
			// llamar a UnequipObject en este mismo instante corta Throw.hkx
			// a mitad y salta a la pose de desarmado -- comprobado en el
			// juego, ver Constants::kThrowReleaseVisualHoldDuration. La
			// réplica (creada más abajo) toma el relevo visual de inmediato,
			// así que ocultar basta para el punto 2 ("el arma original se
			// vuelve invisible"); el desequipado real ("...y se desactiva",
			// necesario para el punto 4: puños libres) se difiere.
			Animation::SetEquippedWeaponHidden(*player, true);

			// Sin cola y aplicación inmediata: un desequipar encolado podía
			// perderse en silencio si se dispara desde un evento de carga
			// (comprobado en la iteración anterior). Diferido el margen de
			// arriba en vez de hacerlo aquí mismo, por el motivo ya
			// explicado. Comprueba el estado al despertar por si el ciclo ya se
			// completó y reinició del todo antes de que venza el margen
			// (arma ya de vuelta en la mano, o una aiming/throwing nueva en
			// marcha) -- en cualquiera de esos casos este desequipado ya
			// quedaría obsoleto y no debe tocar el arma de un ciclo
			// distinto.
			std::thread([this, player, weapon]() {
				std::this_thread::sleep_for(Constants::kThrowReleaseVisualHoldDuration);
				SKSE::GetTaskInterface()->AddTask([this, player, weapon]() {
					const auto state = weaponState.GetState();
					if (state != State::kThrown && state != State::kStuck && state != State::kReturning) {
						return;
					}
					RE::ActorEquipManager::GetSingleton()->UnequipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);

					// Desactivados aquí, junto con el desequipado real, no
					// en OnThrowReleaseAnimationEvent -- ver el comentario
					// de allí.
					Animation::SetAnimationDriven(*player, false);
					Input::SetMovementLocked(false);
				});
			}).detach();

			Throw::LaunchCallbacks callbacks;
			callbacks.onSpawned = [this](RE::ObjectRefHandle a_handle) {
				weaponState.SetActiveReplicaHandle(a_handle);

				// Excepción documentada en TransitionState (ver el header
				// de WeaponManager): en el instante en que ThrowWeapon
				// llamó a TransitionState(kThrown), la réplica todavía no
				// existía -- se engancha aquí, en cuanto el handle real
				// está listo. Comprobado el estado por si el ciclo ya se
				// completó/reinició antes de que el 3D terminara de cargar
				// (mismo criterio que el resto de callbacks async de este
				// archivo).
				if (a_handle.get() && weaponState.GetState() == State::kThrown) {
					Animation::StartMovementVFXOnReplica(a_handle);
				}
			};
			callbacks.onTickStarted = [this](Physics::TickToken a_token) {
				weaponState.SetActiveTickToken(a_token);
			};
			callbacks.onStuck = [this](RE::ActorHandle a_actor) {
				// Comprobado antes de transicionar: el ciclo puede haber
				// cambiado por otra vía (p. ej. el jugador ya pulsó
				// recuperar, o una pantalla de carga resincronizó el
				// estado) antes de que el impacto se detectase.
				if (weaponState.GetState() == State::kThrown) {
					weaponState.SetStuckActorHandle(a_actor);
					TransitionState(State::kStuck);
				}
			};
			callbacks.onAutoRecall = [this]() {
				// Cubre tanto la ida sin impactar (kThrown: distancia
				// máxima, agua) como el objetivo clavado que resulta
				// inmune o supera la duración máxima (kStuck, ver
				// Combat::BeginEmbeddedEffect) — ambos casos son "el
				// ciclo se rinde y recupera solo", nunca ocurren a la vez.
				// Regreso animado (BeginReturn), no recall instantáneo:
				// antes de este fix se teletransportaba a la mano de
				// golpe en vez de volar de vuelta, saltándose la curva
				// del punto 7 (bug detectado en el juego).
				if (weaponState.GetState() == State::kThrown || weaponState.GetState() == State::kStuck) {
					BeginReturn(weaponState.GetState() == State::kStuck);
				}
			};

			Throw::LaunchWeapon(player, weapon->As<RE::TESObjectWEAP>(), std::move(callbacks));
		}

		TransitionState(State::kThrown);
	}

	void WeaponManager::BeginReturn(bool a_wasStuck)
	{
		// Punto 6: "cuando se decide recuperar el arma... libera al
		// objetivo, volviendo al jugador" — se libera de inmediato al
		// iniciar el regreso, no al llegar a la mano.
		if (auto actor = weaponState.GetStuckActorHandle().get()) {
			Combat::EndEmbeddedEffect(actor.get());
		}
		weaponState.SetStuckActorHandle({});

		// El botón de recuperar llega desde fuera de cualquier tick en
		// marcha (a diferencia de la transición ida->clavada, que ocurre
		// dentro del propio tick y se autodetiene devolviendo false) —
		// hay que cancelar aquí el bucle que estuviera controlando la
		// réplica (vuelo, o seguimiento del actor clavado) antes de
		// arrancar el del regreso, o los dos escribirían su posición cada
		// tick (ver Physics::TickToken).
		Physics::CancelTickLoop(weaponState.GetActiveTickToken());
		weaponState.SetActiveTickToken({});

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto  replicaHandle = weaponState.GetActiveReplicaHandle();

		if (!player || !replicaHandle.get()) {
			logs::warn("WeaponManager::BeginReturn: sin jugador o réplica válida, recuperación instantánea de reserva.");
			ReequipAndReset();
			return;
		}

		TransitionState(State::kReturning);

		// Reseteados al arrancar cada regreso -- ver OnPhysicalArrival/
		// OnCatchReleaseAnimationEvent.
		catchPhysicallyArrived = false;
		catchReequipPending = false;

		Return::ReturnCallbacks callbacks;
		callbacks.onTickStarted = [this](Physics::TickToken a_token) {
			weaponState.SetActiveTickToken(a_token);
		};
		callbacks.onApproaching = [this]() {
			BeginCatchAnimation();
		};
		callbacks.onArrived = [this]() {
			// El reequipado real en sí sigue gatillado por la anotación de
			// Catch.hkx (PIE.ThorMjolnirCatch), no por este umbral de
			// distancia -- eso no cambia (ver OnCatchReleaseAnimationEvent),
			// para que el gesto de la mano en el clip siga sincronizado con
			// el reequipado visual. Lo que sí hace este callback (cambio de
			// criterio 2026-08-08, ver CLAUDE.md): confirmar que la llegada
			// física ya ha pasado de verdad -- OnCatchReleaseAnimationEvent
			// comprueba esta confirmación antes de reequipar, y la difiere
			// si todavía no ha llegado (ver catchReequipPending) en vez de
			// confiar ciegamente en que la predicción de tiempo restante de
			// Return::BeginReturnMovement acertara.
			OnPhysicalArrival();
		};

		Return::BeginReturn(player, replicaHandle, a_wasStuck, std::move(callbacks));
	}

	void WeaponManager::RecallWeapon()
	{
		// Punto 6: si la réplica estaba clavada en un actor, liberarlo
		// (quitar la habilidad de parálisis) antes de olvidar el handle —
		// ver Combat::EndEmbeddedEffect.
		if (auto actor = weaponState.GetStuckActorHandle().get()) {
			Combat::EndEmbeddedEffect(actor.get());
		}
		weaponState.SetStuckActorHandle({});

		ReequipAndReset();
	}

	void WeaponManager::ReequipAndReset()
	{
		// Antes de destruir la réplica: si el VFX de movimiento estaba
		// colgado de su nodo (State::kReturning, ver TransitionState), hay
		// que desengancharlo primero -- Physics::DestroyReplica de abajo no
		// lo sabe, y desenganchar de un nodo que el propio Disable/SetDelete
		// ya haya empezado a desmontar es justo el tipo de orden a evitar
		// (ver Animation::StopMovementVFX). TransitionState(kInHand), más
		// abajo, volvería a llamarlo de todas formas, pero ya sin nada que
		// cortar -- no-op inofensivo, mismo criterio que el resto del
		// proyecto.
		Animation::StopMovementVFX();

		Physics::CancelTickLoop(weaponState.GetActiveTickToken());
		weaponState.SetActiveTickToken({});

		Physics::DestroyReplica(weaponState.GetActiveReplicaHandle());
		weaponState.SetActiveReplicaHandle({});

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();

		if (player && weapon) {
			// Se difiere al siguiente tick (tarea de SKSE) en vez de
			// llamarlo aquí mismo: invocado justo al cerrarse una pantalla
			// de carga, el juego aceptaba la orden (sonaba el sonido de
			// equipar) pero nunca llegaba a equipar el arma de verdad
			// (comprobado en la iteración anterior).
			SKSE::GetTaskInterface()->AddTask([player, weapon]() {
				// Suprime la animación completa de equipar/desenvainar al
				// volver el arma a la mano -- vía el mod externo
				// SkipEquipAnimation (dependencia obligatoria del plugin,
				// no de compilación: solo un mod que debe estar instalado y
				// cargado, ver CLAUDE.md). Expone la variable de animation
				// graph "SkipEquipAnimation" (bool), activable con la misma
				// API ya verificada en el punto 2 del plan Kratos
				// (Actor::SetGraphVariableBool, IAnimationGraphManagerHolder).
				// Sustituye por completo al intento anterior de forzar
				// Actor::DrawWeaponMagicHands tras un margen fijo (ver
				// CHANGELOG.md v1.7.6) -- ya no hace falta con la animación
				// suprimida de raíz.
				player->SetGraphVariableBool("SkipEquipAnimation", true);
				RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);

				// Desactivarla en el mismo tick que EquipObject no bastaba
				// (confirmado en el juego: la variable sí se activaba, pero
				// la animación seguía reproduciéndose) -- EquipObject no
				// procesa el equipado de verdad de forma síncrona (mismo
				// motivo por el que ya se difiere un tick, ver más arriba),
				// así que la variable se apagaba antes de que el hook de
				// SkipEquipAnimation llegara a leerla. Se desactiva aparte,
				// tras Constants::kSkipEquipAnimationWindow, mismo patrón
				// hilo-que-duerme-y-reencola de todo el proyecto.
				std::thread([player]() {
					std::this_thread::sleep_for(Constants::kSkipEquipAnimationWindow);
					SKSE::GetTaskInterface()->AddTask([player]() {
						player->SetGraphVariableBool("SkipEquipAnimation", false);
					});
				}).detach();
			});
		}

		weaponState.SetActiveWeapon(nullptr);
		TransitionState(State::kInHand);
	}
}
