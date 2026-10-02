// Ciclo de vida del arma -- ver WeaponManager.h.

#include "3.- WEAPON/WeaponManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "10.- EVENTS/AttackInterruptWatcher.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "12.- AUDIO/CatchSound.h"
#include "12.- AUDIO/SoundResolver.h"
#include "2.- INPUT/InputManager.h"
#include "4.- THROW/ThrowManager.h"
#include "5.- RETURN/ReturnManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/HandGlow.h"
#include "8.- ANIMATION/PowerAttackVFX.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponGlow.h"
#include "8.- ANIMATION/WeaponVFX.h"

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

	void WeaponManager::TransitionState(State a_newState, bool a_manageVfx)
	{
		const auto oldTarget = GetVfxTargetForState(weaponState.GetState());
		weaponState.SetState(a_newState);

		if (!a_manageVfx) {
			return;
		}

		const auto newTarget = GetVfxTargetForState(a_newState);

		if (newTarget == oldTarget) {
			return;
		}

		// Start* ya solapa con el VFX anterior; solo se corta si el destino no lleva VFX.
		switch (newTarget) {
		case VfxTarget::kRealWeapon:
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Animation::StartMovementVFXOnActor(*player);
			}
			break;
		case VfxTarget::kReplica:
			// kThrowing->kThrown: si la réplica aún no existe, ThrowWeapon lo arranca en onSpawned.
			if (auto handle = weaponState.GetActiveReplicaHandle(); handle.get()) {
				Animation::StartMovementVFXOnReplica(handle);
			}
			break;
		case VfxTarget::kNone:
			Animation::StopMovementVFX();
			break;
		}
	}

	void WeaponManager::OnActionButtonDown()
	{
		// Pulsar solo prepara; Lanzar se dispara al soltar. Cada pulsación desarma la anterior.
		throwPressArmed = false;

		if (weaponState.GetState() != State::kInHand) {
			return;
		}

		// Se ignora la pulsación mientras siga pendiente el cierre del ciclo anterior.
		if (throwTailActive || callAnimationActive || catchAnimationActive) {
			return;
		}

		// Con el arma envainada, la primera pulsación solo desenvaina.
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player && !player->AsActorState()->IsWeaponDrawn()) {
			player->DrawWeaponMagicHands(true);
			return;
		}

		throwPressArmed = true;
	}

	void WeaponManager::OnActionButtonUp()
	{
		// Respeta Constants::kMinAttackStartInterval desde el último cambio del grafo.
		const auto elapsedSinceLastAttackEvent = std::chrono::duration<float>(std::chrono::steady_clock::now() - lastAttackAnimationEventTime).count();

		switch (weaponState.GetState()) {
		case State::kInHand:
			{
				// Solo si esta pulsación armó el lanzamiento.
				if (!throwPressArmed) {
					break;
				}
				throwPressArmed = false;

				// Toque demasiado pronto tras el último evento del grafo: se ignora.
				if (elapsedSinceLastAttackEvent < Constants::kMinAttackStartInterval) {
					break;
				}

				// Corta primero un ataque en curso (InterruptAttackThen).
				if (attackInterruptActive || !PrepareThrow()) {
					break;
				}
				InterruptAttackThen([this]() {
					if (weaponState.GetState() == State::kInHand && weaponState.GetActiveWeapon()) {
						BeginThrowAnimation();
					}
				});
				break;
			}
		case State::kThrown:
		case State::kStuck:
			{
				// No se llama mientras el desequipado diferido de Lanzar siga pendiente.
				if (throwTailActive) {
					break;
				}

				// Respeta Constants::kMinAttackStartInterval también al llamar.
				if (elapsedSinceLastAttackEvent < Constants::kMinAttackStartInterval) {
					break;
				}

				// Llamada se dispara al soltar (pulsado escala a power attack) y corta un ataque en curso.
				if (attackInterruptActive) {
					break;
				}
				InterruptAttackThen([this]() {
					const auto state = weaponState.GetState();
					if ((state == State::kThrown || state == State::kStuck) && !throwTailActive) {
						BeginCallAnimation();
					}
				});
				break;
			}
		default:
			break;
		}
	}

	void WeaponManager::ResetToInHand()
	{
		// No hay réplica que borrar al cargar: solo se olvida el handle.
		weaponState.SetActiveWeapon(nullptr);
		weaponState.SetActiveReplicaHandle({});
		weaponState.SetStuckActorHandle({});
		weaponState.SetActiveTickToken({});
		TransitionState(State::kInHand);

		// Solo se revierte iRightHandType si lo cambió nuestro código.
		const bool wasCallAnimationActive = callAnimationActive;
		Scheduler::Cancel(attackInterruptToken);
		Events::AttackInterruptWatcher::Disarm();
		attackInterruptActive = false;
		catchAnimationActive = false;
		catchReequipDone = false;
		catchPhysicallyArrived = false;
		catchReequipPending = false;
		catchEndSoundPlayed = false;
		callAnimationActive = false;
		throwPressArmed = false;

		// Desbloquea movimiento y AnimationDriven por si se cargó en kThrowing.
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
			// Diferido un tick: síncrono falla en silencio.
			SKSE::GetTaskInterface()->AddTask([player, weapon]() {
				RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
			});
		} else {
			logs::warn("WeaponManager::RecoverOrReset: el arma guardada ya no se resuelve, no se reequipa nada.");
		}

		ResetToInHand();
	}

	namespace
	{
		// Formulario buscado por FormID local una vez.
		RE::SpellItem* GetLightningDashSpell()
		{
			static RE::SpellItem* spell = [] {
				auto* dataHandler = RE::TESDataHandler::GetSingleton();
				return dataHandler ? dataHandler->LookupForm<RE::SpellItem>(Constants::kLightningDashSpellLocalFormID, Constants::kSoundPluginName) : nullptr;
			}();
			if (!spell) {
				logs::warn("WeaponManager::GetLightningDashSpell: no se encontró el hechizo (FormID local 0x{:03X}) en \"{}\".",
					Constants::kLightningDashSpellLocalFormID, Constants::kSoundPluginName);
			}
			return spell;
		}

		// Concede o retira Lightning Dash (idempotente).
		void SetLightningDashPower(bool a_granted)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}

			auto* spell = GetLightningDashSpell();
			if (!spell) {
				return;
			}

			const bool hasSpell = player->HasSpell(spell);
			if (a_granted && !hasSpell) {
				player->AddSpell(spell);
			} else if (!a_granted && hasSpell) {
				player->RemoveSpell(spell);
			}
		}
	}

	void WeaponManager::OnThrowableWeaponEquipChanged(bool a_equipped)
	{
		// Durante el ciclo el desequipado es nuestro: el poder se conserva.
		SetLightningDashPower(a_equipped || weaponState.GetState() != State::kInHand);
	}

	void WeaponManager::RestoreLightningDashPower()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (player && ActorUtils::IsThrowableWeaponEquipped(player)) {
			SetLightningDashPower(true);
		}
	}

	void WeaponManager::OnLoadingScreenClosed()
	{
		// Cancela un corte de ataque pendiente.
		Scheduler::Cancel(attackInterruptToken);
		Events::AttackInterruptWatcher::Disarm();
		attackInterruptActive = false;

		switch (weaponState.GetState()) {
		case State::kThrown:
		case State::kStuck:
		case State::kReturning:
			// Regreso en marcha: recuperación instantánea.
			RecallWeapon();
			break;
		case State::kThrowing:
			// En kThrowing basta volver a mano, apagar el Global de OAR y desbloquear el movimiento.
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Animation::SetThrowTrigger(*player, false);
				Animation::SetAnimationDriven(*player, false);
			}
			Input::SetMovementLocked(false);
			TransitionState(State::kInHand);
			break;
		case State::kCalling:
			// En kCalling el arma sigue fuera: RecallWeapon.
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

		// Gesto de Atrape interrumpido: apaga sus flags (trigger, AnimationDriven, bloqueo).
		if (catchAnimationActive) {
			catchAnimationActive = false;
			catchReequipDone = false;
			catchPhysicallyArrived = false;
			catchReequipPending = false;
			catchEndSoundPlayed = false;
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Animation::SetCatchTrigger(*player, false);
				Animation::SetAnimationDriven(*player, false);
				// iRightHandType no se toca: RecallWeapon ya reequipó.
			}
			Input::SetMovementLocked(false);

			// RecallWeapon ya apaga las chispas.
		}

		// Igual para la cola de Llamada.
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

	bool WeaponManager::PrepareThrow()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return false;
		}

		// No se puede lanzar agachado.
		if (player->AsActorState()->IsSneaking()) {
			return false;
		}

		auto* weapon = player->GetEquippedObject(false);
		auto* boundWeapon = weapon ? weapon->As<RE::TESBoundObject>() : nullptr;

		if (!boundWeapon) {
			return false;
		}

		// El arma se fija al soltar, antes de cortar un ataque en curso.
		weaponState.SetActiveWeapon(boundWeapon);
		return true;
	}

	void WeaponManager::BeginThrowAnimation()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		// Da por terminados los efectos de un power attack (instancia única compartida).
		Animation::PowerAttackVFX::Cancel();

		TransitionState(State::kThrowing);

		// Destello siguiendo la mano hasta que exista la réplica.
		Animation::StartWeaponGlow(*player);

		// Brillo de manos.
		Animation::TriggerHandGlow(*player);

		// Bloquea el movimiento durante Lanzar para que no escale a power attack direccional.
		Input::SetMovementLocked(true);

		// Activa la graph variable vanilla AnimationDriven.
		Animation::SetAnimationDriven(*player, true);

		// El Global hace que OAR sustituya el ataque ligero por Throw.hkx.
		Animation::SetThrowTrigger(*player, true);

		// Si el grafo rechaza el evento, la animación no se verá (conflicto con otro behavior).
		if (!player->NotifyAnimationGraph(Constants::kLightAttackAnimationEvent)) {
			logs::warn("WeaponManager: el grafo de animación rechazó '{}' para Lanzar; el arma saldrá por la red de seguridad.", Constants::kLightAttackAnimationEvent);
		}

		// Red de seguridad: el lanzamiento ocurre aunque no llegue la anotación.
		(void)Scheduler::After(Constants::kThrowReleaseFallbackWindow, [this]() {
			if (weaponState.GetState() == State::kThrowing) {
				logs::warn("WeaponManager: la anotación de Throw.hkx no llegó (red de seguridad). Revisa que Open Animation Replacer y el submod de ThorMjolnir estén activos.");
			}
			OnThrowReleaseAnimationEvent();
		});
	}

	void WeaponManager::InterruptAttackThen(std::function<void()> a_action)
	{
		auto*      player = RE::PlayerCharacter::GetSingleton();
		const auto attackState = player ? player->AsActorState()->GetAttackState() : RE::ATTACK_STATE_ENUM::kNone;
		if (attackState == RE::ATTACK_STATE_ENUM::kNone) {
			// Bloqueo en curso: attackStart no tiene transición desde BlockState.
			if (player && player->IsBlocking()) {
				InterruptBlockThen(*player, std::move(a_action));
				return;
			}
			a_action();
			return;
		}

		// Un único disparo entre el evento y la red de seguridad.
		auto pending = std::make_shared<std::atomic<bool>>(true);
		attackInterruptToken = pending;
		attackInterruptActive = true;

		auto fire = [this, pending, action = std::move(a_action)]() {
			if (!pending->exchange(false)) {
				return;
			}
			Events::AttackInterruptWatcher::Disarm();
			attackInterruptActive = false;
			action();
		};

		// Hilo principal; espera además kAttackInterruptPostEventDelay.
		Events::AttackInterruptWatcher::Arm(*player, [fire]() {
			(void)Scheduler::After(Constants::kAttackInterruptPostEventDelay, fire);
		});

		player->NotifyAnimationGraph(Constants::kAttackStopAnimationEvent);

		(void)Scheduler::After(Constants::kAttackInterruptFallbackDelay, fire);
	}

	void WeaponManager::InterruptBlockThen(RE::PlayerCharacter& a_player, std::function<void()> a_action)
	{
		auto pending = std::make_shared<std::atomic<bool>>(true);
		attackInterruptToken = pending;
		attackInterruptActive = true;

		a_player.NotifyAnimationGraph(Constants::kBlockStopInstantAnimationEvent);

		(void)Scheduler::After(Constants::kBlockInterruptSettleDelay, [this, pending, action = std::move(a_action)]() {
			if (!pending->exchange(false)) {
				return;
			}
			attackInterruptActive = false;
			action();
		});
	}

	void WeaponManager::OnThrowReleaseAnimationEvent()
	{
		if (weaponState.GetState() != State::kThrowing) {
			return;
		}

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetThrowTrigger(*player, false);
		}

		// Movimiento y AnimationDriven se desbloquean con el desequipado real (ThrowWeapon).
		ThrowWeapon();
	}

	void WeaponManager::BeginCallAnimation()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		// Se guarda antes de pasar a kCalling.
		wasStuckBeforeCalling = weaponState.GetState() == State::kStuck;
		TransitionState(State::kCalling);
		callAnimationActive = true;

		// Bloquea el movimiento durante Llamada.
		Input::SetMovementLocked(true);
		Animation::SetAnimationDriven(*player, true);

		// iRightHandType a "una mano" para que la rama de combate reproduzca Call.hkx.
		player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, Constants::kRightHandTypeOneHanded);

		Animation::SetCallTrigger(*player, true);

		if (!player->NotifyAnimationGraph(Constants::kLightAttackAnimationEvent)) {
			logs::warn("WeaponManager: el grafo de animación rechazó '{}' para Llamada; el regreso empezará por la red de seguridad.", Constants::kLightAttackAnimationEvent);
		}

		// Red de seguridad: el regreso empieza aunque no llegue la anotación.
		(void)Scheduler::After(Constants::kCallReleaseFallbackWindow, [this]() {
			if (weaponState.GetState() == State::kCalling) {
				logs::warn("WeaponManager: la anotación de Call.hkx no llegó (red de seguridad). Revisa que Open Animation Replacer y el submod de ThorMjolnir estén activos.");
			}
			OnCallReleaseAnimationEvent();
		});
	}

	void WeaponManager::OnCallReleaseAnimationEvent()
	{
		if (weaponState.GetState() != State::kCalling) {
			return;
		}

		logs::info("[DIAG] Llamada soltada (clavada {})", wasStuckBeforeCalling);

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			// Chasquido en el mismo instante que el regreso.
			Audio::PlayFileOneShot(player->GetPosition(), Constants::kCallReleaseSoundFilePath, Constants::kCallReleaseSoundVolume);
		}

		// El resto del gesto se difiere a FinishCallAnimation.
		BeginReturn(wasStuckBeforeCalling);

		// FinishCallAnimation ya comprueba callAnimationActive.
		(void)Scheduler::After(Constants::kCallAnimationTailDuration, [this]() {
			FinishCallAnimation();
		});
	}

	void WeaponManager::FinishCallAnimation()
	{
		if (!callAnimationActive) {
			// Ya limpiado por otra vía.
			return;
		}
		logs::info("[DIAG] FinishCallAnimation (attackStop, movimiento desbloqueado)");
		callAnimationActive = false;

		// Bandera y timestamp juntos, para que una pulsación no lea un timestamp viejo.
		lastAttackAnimationEventTime = std::chrono::steady_clock::now();

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetCallTrigger(*player, false);
			Animation::SetAnimationDriven(*player, false);

			// Vuelve iRightHandType a 0 (desarmado).
			player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, 0);

			// attackStop pasada la cola de Call.hkx para desatascar el grafo.
			player->NotifyAnimationGraph(Constants::kAttackStopAnimationEvent);
		}
		Input::SetMovementLocked(false);
	}

	void WeaponManager::BeginCatchAnimation()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			// Sin jugador: recuperación instantánea y fundido de chispas.
			ReequipAndReset();
			Animation::FadeOutMovementVFX();
			Animation::StopWeaponGlow();
			return;
		}
		if (catchAnimationActive) {
			// Evita disparar el gesto dos veces.
			return;
		}

		// No toca weaponState: el gesto se sigue con catchAnimationActive.
		catchAnimationActive = true;
		catchReequipDone = false;
		catchEndSoundPlayed = false;

		// Bloquea el movimiento durante Atrape.
		Input::SetMovementLocked(true);
		Animation::SetAnimationDriven(*player, true);

		// iRightHandType a "una mano"; el arma real aún no está equipada.
		player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, Constants::kRightHandTypeOneHanded);

		Animation::SetCatchTrigger(*player, true);

		// Brillo de manos.
		Animation::TriggerHandGlow(*player);

		const bool accepted = player->NotifyAnimationGraph(Constants::kLightAttackAnimationEvent);
		logs::info("[DIAG] BeginCatchAnimation: attackStart aceptado {}", accepted);
		if (accepted) {
			// Catch.hkx empieza ahora: el arma debe llegar a su anotación; se mide cuánto tarda.
			catchAnimationStartTime = std::chrono::steady_clock::now();
			catchLeadMeasurePending = true;
			if (catchSync) {
				catchSync->OnCatchStarted();
			}
		} else {
			logs::warn("WeaponManager: el grafo de animación rechazó '{}' para Atrape; el reequipado llegará por la red de seguridad.", Constants::kLightAttackAnimationEvent);
			if (catchSync) {
				catchSync->Release();
			}
		}

		// Red de seguridad: el reequipado ocurre aunque no llegue la anotación
		// (kCatchReleaseFallbackWindow > kCatchAnimationLeadTime).
		(void)Scheduler::After(Constants::kCatchReleaseFallbackWindow, [this]() {
			if (catchAnimationActive) {
				logs::warn("WeaponManager: la anotación de Catch.hkx no llegó (red de seguridad). Revisa que Open Animation Replacer y el submod de ThorMjolnir estén activos.");
			}
			OnCatchReleaseAnimationEvent(false);
		});
	}

	void WeaponManager::OnCatchReleaseAnimationEvent(bool a_fromAnnotation)
	{
		logs::info("[DIAG] OnCatchReleaseAnimationEvent: anotación {}, gesto activo {}, reequipado hecho {}, llegada física {}", a_fromAnnotation, catchAnimationActive, catchReequipDone, catchPhysicallyArrived);
		if (!catchAnimationActive || catchReequipDone) {
			return;
		}

		// Tiempo real de Catch.hkx hasta su anotación, para fijar la llegada del próximo Atrape.
		if (a_fromAnnotation && catchLeadMeasurePending) {
			catchLeadMeasurePending = false;
			const float measured = std::chrono::duration<float>(std::chrono::steady_clock::now() - catchAnimationStartTime).count();
			const float nominal = Constants::kCatchAnimationLeadTime;
			if (measured >= nominal * Constants::kCatchLeadMeasureMinFactor && measured <= nominal * Constants::kCatchLeadMeasureMaxFactor) {
				catchLeadSeconds = measured;
				logs::info("[DIAG] Catch.hkx hasta su anotación: {:.3f} s", measured);
			} else {
				logs::warn("WeaponManager: medida de Catch.hkx fuera de rango ({:.3f} s), se conserva {:.3f} s.", measured, catchLeadSeconds);
			}
		}

		// Sonido final en el instante de la anotación, antes de esperar la llegada física;
		// catchEndSoundPlayed evita repetirlo.
		if (!catchEndSoundPlayed) {
			catchEndSoundPlayed = true;
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Audio::CatchCue::PlayEnd(player->GetPosition());
			}
		}

		// Si la réplica aún no llegó, el reequipado espera a OnPhysicalArrival.
		if (!catchPhysicallyArrived) {
			catchReequipPending = true;
			return;
		}

		PerformCatchReequip();
	}

	void WeaponManager::OnPhysicalArrival()
	{
		logs::info("[DIAG] OnPhysicalArrival: reequipado pendiente de la anotación {}", catchReequipPending);
		catchPhysicallyArrived = true;
		if (catchReequipPending) {
			catchReequipPending = false;
			PerformCatchReequip();
		}
	}

	void WeaponManager::PerformCatchReequip()
	{
		logs::info("[DIAG] PerformCatchReequip");
		catchReequipDone = true;

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			// Temblor de cámara al cerrar la mano, con epicentro en el jugador.
			RE::ShakeCamera(Constants::kCatchShakeStrength, player->GetPosition(), Constants::kCatchShakeDuration);
		}

		// Reequipa con la anotación de Catch.hkx y las chispas pasan a seguir la mano.
		// iRightHandType no se toca: ya vale "una mano".
		ReequipAndReset(true);

		// El resto del gesto se difiere a FinishCatchAnimation (kCatchAnimationTailDuration).
		(void)Scheduler::After(Constants::kCatchAnimationTailDuration, [this]() {
			FinishCatchAnimation();
		});
	}

	void WeaponManager::FinishCatchAnimation()
	{
		if (!catchAnimationActive) {
			// Ya limpiado por otra vía.
			return;
		}
		logs::info("[DIAG] FinishCatchAnimation (attackStop, movimiento desbloqueado)");
		catchAnimationActive = false;
		catchReequipDone = false;
		catchPhysicallyArrived = false;
		catchReequipPending = false;
		catchEndSoundPlayed = false;

		// Bandera y timestamp juntos, para que una pulsación no lea un timestamp viejo.
		lastAttackAnimationEventTime = std::chrono::steady_clock::now();

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetCatchTrigger(*player, false);
			Animation::SetAnimationDriven(*player, false);

			// attackStop pasada la cola de Catch.hkx para desatascar el grafo.
			player->NotifyAnimationGraph(Constants::kAttackStopAnimationEvent);
		}
		Input::SetMovementLocked(false);

		// Apaga las chispas al final del Atrape, tras kCatchVfxSettleDelay.
		Animation::FadeOutMovementVFX(true);
		Animation::StopWeaponGlow();
	}

	void WeaponManager::EquipGestureWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();
		if (!player || !weapon) {
			return;
		}

		// EquipGuard no deshace este equipado.
		suppressEquipGuard = true;

		// Sin animación de desenvainar (SkipEquipAnimation).
		player->SetGraphVariableBool("SkipEquipAnimation", true);
		RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);

		// No se oculta aún: el equipado no está listo en este instante.

		// SkipEquipAnimation y la supresión de EquipGuard se apagan pasado kSkipEquipAnimationWindow.
		(void)Scheduler::After(Constants::kSkipEquipAnimationWindow, [this, player]() {
			player->SetGraphVariableBool("SkipEquipAnimation", false);
			suppressEquipGuard = false;
		});
	}

	void WeaponManager::UnequipGestureWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();
		if (!player || !weapon) {
			return;
		}

		// EquipGuard solo reacciona a equipados.
		player->SetGraphVariableBool("SkipEquipAnimation", true);
		RE::ActorEquipManager::GetSingleton()->UnequipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);

		// (void): nada cancela esta ventana desde fuera todavía.
		(void)Scheduler::After(Constants::kSkipEquipAnimationWindow, [player]() {
			player->SetGraphVariableBool("SkipEquipAnimation", false);
		});
	}

	void WeaponManager::ThrowWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();

		if (player && weapon) {
			// Se oculta el arma y la réplica toma el relevo; el desequipado real se difiere.
			Animation::SetEquippedWeaponHidden(*player, true);
			throwTailActive = true;
			lastAttackAnimationEventTime = std::chrono::steady_clock::now();

			// Desequipado real pasado kThrowReleaseVisualHoldDuration (cancelable con throwTailToken).
			throwTailToken = Scheduler::After(Constants::kThrowReleaseVisualHoldDuration, [this, player, weapon]() {
				throwTailActive = false;

				RE::ActorEquipManager::GetSingleton()->UnequipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);

				// Solo desbloquea movimiento y AnimationDriven si Llamada o Atrape no los han tomado.
				if (!callAnimationActive && !catchAnimationActive) {
					Animation::SetAnimationDriven(*player, false);
					Input::SetMovementLocked(false);
				}
			});

			Throw::LaunchCallbacks callbacks;
			callbacks.onSpawned = [this](RE::ObjectRefHandle a_handle) {
				weaponState.SetActiveReplicaHandle(a_handle);

				// La réplica ya existe: arranca sus chispas si el ciclo sigue en kThrown.
				if (a_handle.get() && weaponState.GetState() == State::kThrown) {
					Animation::StartMovementVFXOnReplica(a_handle);

					// El destello pasa a seguir la réplica.
					Animation::RetargetWeaponGlowToReplica(a_handle);
				}
			};
			callbacks.onTickStarted = [this](Physics::TickToken a_token) {
				weaponState.SetActiveTickToken(a_token);
			};
			callbacks.onStuck = [this](RE::ActorHandle a_actor) {
				// Solo si el ciclo sigue en kThrown.
				if (weaponState.GetState() == State::kThrown) {
					weaponState.SetStuckActorHandle(a_actor);

					// Las chispas se apagan con fundido; a_manageVfx=false para no cortarlas.
					TransitionState(State::kStuck, false);
					Animation::FadeOutMovementVFX();
				}
			};
			callbacks.onAutoRecall = [this]() {
				// Agua, objetivo inmune o tiempo máximo clavada: el arma vuelve sola (regreso animado).
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
		// Libera al objetivo al iniciar el regreso.
		if (auto actor = weaponState.GetStuckActorHandle().get()) {
			Combat::EndEmbeddedEffect(actor.get());
		}
		weaponState.SetStuckActorHandle({});

		// Quita el hazard al desclavar.
		Combat::RemoveImpactHazard();

		// Cancela el bucle que movía la réplica antes de arrancar el del regreso.
		Physics::CancelTickLoop(weaponState.GetActiveTickToken());
		weaponState.SetActiveTickToken({});

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto  replicaHandle = weaponState.GetActiveReplicaHandle();

		// Devuelve la colisión a la réplica.
		Combat::RestoreReplicaCollision(replicaHandle.get().get());

		if (!player || !replicaHandle.get()) {
			logs::warn("WeaponManager::BeginReturn: sin jugador o réplica válida, recuperación instantánea de reserva.");
			// Sin regreso: fundido inmediato.
			ReequipAndReset();
			Animation::FadeOutMovementVFX();
			Animation::StopWeaponGlow();
			return;
		}

		TransitionState(State::kReturning);

		// Se reinician en cada regreso.
		catchPhysicallyArrived = false;
		catchReequipPending = false;

		Return::ReturnCallbacks callbacks;
		callbacks.onTickStarted = [this](Physics::TickToken a_token) {
			weaponState.SetActiveTickToken(a_token);
		};
		callbacks.onApproaching = [this]() {
			if (auto* diagPlayer = RE::PlayerCharacter::GetSingleton()) {
				logs::info("[DIAG] onApproaching -> InterruptAttackThen: attackState {}, bloqueando {}",
					static_cast<std::uint32_t>(diagPlayer->AsActorState()->GetAttackState()), diagPlayer->IsBlocking());
			}
			// Con el bloqueo pulsado, corta el bloqueo antes del gesto de Atrape.
			InterruptAttackThen([this]() {
				if (weaponState.GetState() == State::kReturning) {
					BeginCatchAnimation();
				}
			});
		};
		callbacks.onArrived = [this]() {
			// Confirma la llegada física; completa un reequipado pendiente.
			OnPhysicalArrival();
		};

		catchSync = std::make_shared<Return::CatchSync>(catchLeadSeconds);
		Return::BeginReturn(player, replicaHandle, a_wasStuck, catchSync, std::move(callbacks));
	}

	void WeaponManager::RecallWeapon()
	{
		// Libera al actor clavado.
		if (auto actor = weaponState.GetStuckActorHandle().get()) {
			Combat::EndEmbeddedEffect(actor.get());
		}
		weaponState.SetStuckActorHandle({});
		Combat::RemoveImpactHazard();
		// Olvida la capa guardada.
		Combat::RestoreReplicaCollision(nullptr);

		// Sin animación: fundido inmediato.
		ReequipAndReset();
		Animation::FadeOutMovementVFX();
		Animation::StopWeaponGlow();
	}

	void WeaponManager::ReequipAndReset(bool a_reattachVfxToHand)
	{
		// Cancela el desequipado diferido de Lanzar si seguía pendiente.
		Scheduler::Cancel(throwTailToken);
		throwTailActive = false;

		// Las chispas no se apagan aquí; con a_reattachVfxToHand pasan a seguir la mano.
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (a_reattachVfxToHand && player) {
			Animation::RetargetMovementVFXToActor(*player);
			Animation::RetargetWeaponGlowToActor(*player);
		}

		Physics::CancelTickLoop(weaponState.GetActiveTickToken());
		weaponState.SetActiveTickToken({});
		catchSync.reset();

		Physics::DestroyReplica(weaponState.GetActiveReplicaHandle());
		weaponState.SetActiveReplicaHandle({});

		auto* weapon = weaponState.GetActiveWeapon();

		if (player && weapon) {
			// Diferido un tick: tras una pantalla de carga, síncrono no equipa.
			SKSE::GetTaskInterface()->AddTask([this, player, weapon]() {
				// Sin animación de equipar/desenvainar (graph variable "SkipEquipAnimation").
				player->SetGraphVariableBool("SkipEquipAnimation", true);
				RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);

				// Se apaga pasado kSkipEquipAnimationWindow; cancela el temporizador anterior.
				Scheduler::Cancel(skipEquipAnimationToken);
				skipEquipAnimationToken = Scheduler::After(Constants::kSkipEquipAnimationWindow, [player]() {
					player->SetGraphVariableBool("SkipEquipAnimation", false);
				});
			});
		}

		weaponState.SetActiveWeapon(nullptr);
		TransitionState(State::kInHand, false);
	}
}
