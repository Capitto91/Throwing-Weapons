// Implementación del ciclo de vida del arma.
// Coordina la transición entre arma en mano, arma lanzada y arma recuperada.

#include "3.- WEAPON/WeaponManager.h"

#include "1.- CORE/Constants.h"
#include "2.- INPUT/InputManager.h"
#include "4.- THROW/ThrowManager.h"
#include "5.- RETURN/ReturnManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "10.- EVENTS/EventManager.h"

#include <thread>

namespace Weapon
{
	WeaponManager* WeaponManager::GetSingleton()
	{
		static WeaponManager singleton;
		return &singleton;
	}

	void WeaponManager::OnAimButtonDown()
	{
		switch (weaponState.GetState()) {
		case State::kInHand: {
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
			weaponState.SetState(State::kInHand);
			BeginAiming();
			break;
		case State::kThrown:
		case State::kStuck:
			BeginReturn();
			break;
		default:
			break;
		}
	}

	void WeaponManager::OnAimButtonUp()
	{
		if (weaponState.GetState() == State::kAiming) {
			BeginThrowAnimation();
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
		weaponState.SetState(State::kInHand);

		// Por si el estado anterior era kThrowing (partida guardada/cargada
		// a mitad de esa ventana, dentro de la misma sesión del proceso):
		// sin esto, el movimiento se quedaría bloqueado para siempre. Llamar
		// aquí sin comprobar el estado previo es seguro -- desbloquear un
		// movimiento que ya estaba desbloqueado es un no-op inofensivo (ver
		// Input::SetMovementLocked).
		Input::SetMovementLocked(false);
	}

	WeaponManager::SaveCycleData WeaponManager::CaptureSaveData() const
	{
		SaveCycleData data;
		data.cycleActive = weaponState.GetState() == State::kThrown ||
		                   weaponState.GetState() == State::kStuck ||
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
			weaponState.SetState(State::kInHand);
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
			weaponState.SetState(State::kInHand);
			break;
		default:
			break;
		}
	}

	void WeaponManager::BeginAiming()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = player ? player->GetEquippedObject(false) : nullptr;
		auto* boundWeapon = weapon ? weapon->As<RE::TESBoundObject>() : nullptr;

		if (!boundWeapon) {
			return;
		}

		// Registro perezoso del sink de liberación de Lanzar (ver
		// EventManager.h): garantiza que ya está enganchado antes de que
		// pueda hacer falta, sin depender de kNewGame/kPostLoadGame -- que
		// `coc` desde la consola del menú principal se salta por completo.
		Events::EnsureAnimationSinksRegistered();

		weaponState.SetActiveWeapon(boundWeapon);
		weaponState.SetState(State::kAiming);

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

		weaponState.SetState(State::kThrowing);

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

		// Fase 3 del plan OAR (_reference/PLAN-OAR.md): la graph variable
		// gatea el submod de OAR que sustituye Constants::kThrowAnimationEvent
		// (un evento vanilla ya existente, ninguno nuevo) por Throw.hkx.
		Animation::SetThrowTrigger(*player, true);
		const bool notifyOk = player->NotifyAnimationGraph(Constants::kThrowAnimationEvent);

		// Log temporal de diagnóstico (retirar una vez confirmado en el
		// juego el ciclo real, ver _reference/PLAN-OAR.md Fase 3): confirma
		// que la escritura de la graph variable se lee de vuelta como se
		// espera, y si NotifyAnimationGraph reporta éxito, antes de disparar
		// el evento.
		float readBack = -1.0f;
		player->GetGraphVariableFloat(Constants::kThrowTriggerGraphVariable, readBack);
		logs::info("WeaponManager::BeginThrowAnimation: graph variable '{}' puesta a 1, releída como {} -- '{}' disparado, NotifyAnimationGraph()={}.",
			Constants::kThrowTriggerGraphVariable, readBack, Constants::kThrowAnimationEvent, notifyOk);

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
			Animation::SetAnimationDriven(*player, false);
		}
		Input::SetMovementLocked(false);

		ThrowWeapon();
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
				});
			}).detach();

			Throw::LaunchCallbacks callbacks;
			callbacks.onSpawned = [this](RE::ObjectRefHandle a_handle) {
				weaponState.SetActiveReplicaHandle(a_handle);
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
					weaponState.SetState(State::kStuck);
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
					BeginReturn();
				}
			};

			Throw::LaunchWeapon(player, weapon->As<RE::TESObjectWEAP>(), std::move(callbacks));
		}

		weaponState.SetState(State::kThrown);
	}

	void WeaponManager::BeginReturn()
	{
		// Punto 11: si el arma estaba clavada (superficie o actor) en el
		// instante de pulsar recuperar, el temblor de desprendimiento debe
		// reproducirse antes del movimiento de vuelta -- capturado antes de
		// tocar el estado, ver Return::BeginReturn.
		const bool wasStuck = weaponState.GetState() == State::kStuck;

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

		weaponState.SetState(State::kReturning);

		Return::ReturnCallbacks callbacks;
		callbacks.onTickStarted = [this](Physics::TickToken a_token) {
			weaponState.SetActiveTickToken(a_token);
		};
		callbacks.onArrived = [this]() {
			ReequipAndReset();
		};

		Return::BeginReturn(player, replicaHandle, wasStuck, std::move(callbacks));
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
		weaponState.SetState(State::kInHand);
	}
}
