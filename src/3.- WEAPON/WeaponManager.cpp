// Implementación del ciclo de vida del arma.
// Coordina la transición entre arma en mano, arma lanzada y arma recuperada.

#include "3.- WEAPON/WeaponManager.h"

#include "4.- THROW/ThrowManager.h"
#include "5.- RETURN/ReturnManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "7.- COMBAT/DamageManager.h"

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
		case State::kInHand:
			BeginAiming();
			break;
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
			ThrowWeapon();
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

		weaponState.SetActiveWeapon(boundWeapon);
		weaponState.SetState(State::kAiming);
	}

	void WeaponManager::ThrowWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();

		if (player && weapon) {
			// Sin cola y aplicación inmediata: un desequipar encolado podía
			// perderse en silencio si se dispara desde un evento de carga
			// (comprobado en la iteración anterior).
			RE::ActorEquipManager::GetSingleton()->UnequipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);

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

		Return::BeginReturn(player, replicaHandle, std::move(callbacks));
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
				RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
			});
		}

		weaponState.SetActiveWeapon(nullptr);
		weaponState.SetState(State::kInHand);
	}
}
