// Ciclo de vida del arma -- ver WeaponManager.h.

#include "3.- WEAPON/WeaponManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Forms.h"
#include "1.- CORE/FrameHook.h"
#include "1.- CORE/Scheduler.h"
#include "10.- EVENTS/AttackInterruptWatcher.h"
#include "10.- EVENTS/GraphSettleWatcher.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "12.- AUDIO/CatchSound.h"
#include "12.- AUDIO/SoundResolver.h"
#include "2.- INPUT/InputManager.h"
#include "3.- WEAPON/LightningDash.h"
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

		// Nombre del gesto para el log.
		const char* GestureName(Animation::Gesture a_gesture)
		{
			switch (a_gesture) {
			case Animation::Gesture::kThrow:
				return "Lanzar";
			case Animation::Gesture::kCall:
				return "Llamada";
			case Animation::Gesture::kCatch:
				return "Atrape";
			default:
				return "golpe en salto";
			}
		}

		// Red de seguridad de un gesto: pasado a_window ejecuta a_release (que ignora la llamada si la anotación ya llegó)
		// y, si a_stillPending, avisa de que la anotación de a_clip no llegó.
		void ScheduleReleaseFallback(std::chrono::milliseconds a_window, const char* a_clip, std::function<bool()> a_stillPending, std::function<void()> a_release)
		{
			(void)Scheduler::After(a_window, [a_clip, stillPending = std::move(a_stillPending), release = std::move(a_release)]() {
				if (stillPending()) {
					logs::warn("WeaponManager: la anotación de {} no llegó (red de seguridad). Revisa que Open Animation Replacer y el submod de ThorMjolnir estén activos.", a_clip);
				}
				release();
			});
		}

		// Oculta o muestra a_weapon en los menús (inventario, cofres, comercio, favoritos) con la marca "no jugable" de su
		// registro, solo en memoria: lanzada no se puede soltar, vender, guardar ni equipar. La usan ThrowWeapon y los regresos.
		void SetHiddenFromMenus(RE::TESBoundObject* a_weapon, bool a_hidden)
		{
			if (auto* weapon = a_weapon ? a_weapon->As<RE::TESObjectWEAP>() : nullptr) {
				weapon->weaponData.flags.set(a_hidden, RE::TESObjectWEAP::Data::Flag::kNonPlayable);
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

		if (weaponState.GetState() != State::kInHand || LightningDash::IsActive()) {
			return;
		}

		// Se ignora la pulsación mientras siga pendiente el cierre del ciclo anterior.
		if (throwTailActive || callAnimationActive || catchState.active) {
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
		// Durante Lightning Dash la tecla no llama ni lanza; la llegada recupera el arma.
		if (LightningDash::IsActive()) {
			return;
		}

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
		// Un Lightning Dash en curso se corta (devuelve el movimiento).
		LightningDash::Cancel();

		// Estado: no hay réplica que borrar al cargar, solo se olvida el handle; el arma vuelve a los menús.
		SetHiddenFromMenus(weaponState.GetActiveWeapon(), false);
		weaponState.SetActiveWeapon(nullptr);
		weaponState.SetActiveReplicaHandle({});
		weaponState.SetStuckActorHandle({});
		weaponState.SetStuckSurfaceNormal({});
		weaponState.SetActiveTickToken({});
		TransitionState(State::kInHand);

		// Gestos y temporizadores del ciclo anterior: ninguno se ejecuta en la partida cargada.
		// iRightHandType solo se revierte si lo cambió nuestro código.
		const bool wasCallAnimationActive = callAnimationActive;
		Scheduler::Cancel(attackInterruptToken);
		Scheduler::Cancel(throwTailToken);
		Events::AttackInterruptWatcher::Disarm();
		attackInterruptActive = false;
		throwTailActive = false;
		catchState = {};
		callAnimationActive = false;
		throwPressArmed = false;

		// Efectos de antes de la carga, en el acto: los del ataque fuerte, las chispas y el destello.
		Animation::PowerAttackVFX::Cancel();
		Animation::StopMovementVFX();
		Animation::StopWeaponGlowNow();

		// Desbloquea movimiento, cambio de cámara y AnimationDriven y apaga los Globals de los gestos (la partida guarda
		// su valor), por si se cargó a mitad de un gesto o de un dash.
		Input::SetMovementLocked(false);
		Input::SetCameraSwitchLocked(false);
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetAnimationDriven(*player, false);
			Animation::SetTrigger(Animation::Gesture::kThrow, false);
			Animation::SetTrigger(Animation::Gesture::kCall, false);
			Animation::SetTrigger(Animation::Gesture::kCatch, false);
			Animation::SetTrigger(Animation::Gesture::kSlam, false);
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
				Physics::DestroyReference(RE::ObjectRefHandle(replicaRefr));
			}
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weaponForm = a_data.weaponFormID ? RE::TESForm::LookupByID(a_data.weaponFormID) : nullptr;
		auto* weapon = weaponForm ? weaponForm->As<RE::TESBoundObject>() : nullptr;

		if (player && weapon) {
			// De vuelta en los menús (la marca puede seguir en memoria) y reequipada diferida un tick: síncrono falla en silencio.
			SetHiddenFromMenus(weapon, false);
			SKSE::GetTaskInterface()->AddTask([this, player, weapon]() {
				if (!ActorUtils::EquipNow(*player, weapon)) {
					logs::warn("WeaponManager::RecoverOrReset: el arma ya no está en el inventario, no se reequipa.");
					OnThrowableWeaponEquipChanged(false);
				}
			});
		} else {
			logs::warn("WeaponManager::RecoverOrReset: el arma guardada ya no se resuelve, no se reequipa nada.");
		}

		ResetToInHand();
	}

	namespace
	{
		// Concede o retira Lightning Dash (idempotente).
		void SetLightningDashPower(bool a_granted)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}

			auto* spell = Forms::lightningDashSpell;
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

		// Motivo por el que Lightning Dash no puede desplazar al jugador (kNone si puede).
		enum class DashBlock
		{
			kNone,
			kSilent,  // ya en marcha o sin réplica: sin aviso
			kInHand,
			kReturning,
			kCooldown,
			kTooFar
		};

		DashBlock GetDashBlock(RE::PlayerCharacter& a_player, State a_state, RE::TESObjectREFR* a_replica)
		{
			if (LightningDash::IsActive()) {
				return DashBlock::kSilent;
			}

			switch (a_state) {
			case State::kInHand:
			case State::kThrowing:
				return DashBlock::kInHand;
			case State::kCalling:
			case State::kReturning:
				return DashBlock::kReturning;
			default:
				break;
			}

			if (!a_replica) {
				return DashBlock::kSilent;
			}
			if (LightningDash::IsOnCooldown(a_player)) {
				return DashBlock::kCooldown;
			}
			if (a_player.GetPosition().GetDistance(a_replica->GetPosition()) > Constants::kLightningDashMaxDistance) {
				return DashBlock::kTooFar;
			}
			return DashBlock::kNone;
		}

		// Aviso en pantalla del motivo, sin repetir el mismo antes de kLightningDashMessageRepeatSeconds.
		// Diferido con Scheduler: CastHook lo pide desde dentro de la comprobación del lanzamiento.
		void ShowDashBlockMessage(DashBlock a_block)
		{
			const char* message = nullptr;
			switch (a_block) {
			case DashBlock::kInHand:
				message = Constants::kLightningDashInHandMessage;
				break;
			case DashBlock::kReturning:
				message = Constants::kLightningDashReturningMessage;
				break;
			case DashBlock::kCooldown:
				message = Constants::kLightningDashCooldownMessage;
				break;
			case DashBlock::kTooFar:
				message = Constants::kLightningDashTooFarMessage;
				break;
			default:
				return;
			}

			static DashBlock lastBlock = DashBlock::kNone;
			static double    lastTime = 0.0;
			const double     now = FrameHook::Now();
			if (a_block == lastBlock && now - lastTime < Constants::kLightningDashMessageRepeatSeconds) {
				return;
			}
			lastBlock = a_block;
			lastTime = now;

			logs::info("WeaponManager: Lightning Dash denegado: \"{}\".", message);

			(void)Scheduler::After(std::chrono::milliseconds{ 0 }, [message]() {
				RE::SendHUDMessage::ShowHUDMessage(message);
			});
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

	bool WeaponManager::CanCastLightningDash()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return true;
		}

		auto       replica = weaponState.GetActiveReplicaHandle().get();
		const auto block = GetDashBlock(*player, weaponState.GetState(), replica.get());
		ShowDashBlockMessage(block);
		return block == DashBlock::kNone;
	}

	void WeaponManager::OnLightningDashCast()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			return;
		}

		// Misma comprobación que CastHook, por si el estado cambió entre medias.
		auto       replica = weaponState.GetActiveReplicaHandle().get();
		const auto state = weaponState.GetState();
		const auto block = GetDashBlock(*player, state, replica.get());
		if (block != DashBlock::kNone) {
			ShowDashBlockMessage(block);
			return;
		}

		// Clavada: delante del actor o separada de la superficie. En vuelo: el punto donde está ahora.
		const bool         stuck = state == State::kStuck;
		auto               stuckActor = stuck ? weaponState.GetStuckActorHandle().get() : RE::NiPointer<RE::Actor>{};
		const RE::NiPoint3 surfaceNormal = stuck ? weaponState.GetStuckSurfaceNormal() : RE::NiPoint3{};
		const auto         destination = LightningDash::ComputeDestination(*player, replica->GetPosition(), stuckActor.get(), surfaceNormal);

		// Golpe en salto: solo con el arma en vuelo y la llegada a menos de kLightningDashSlamMaxHeight del suelo.
		const auto slamGround = state == State::kThrown ? LightningDash::FindSlamGround(*player, destination, replica.get()) : std::nullopt;

		// En vuelo, el arma se queda en ese punto y desaparece hasta la llegada: sin seguir su curso ni chocar entretanto.
		// La llegada (o una pantalla de carga) la recupera y destruye la réplica.
		if (state == State::kThrown && replica) {
			weaponState.CancelTickLoop();
			Animation::FadeOutMovementVFX();
			Animation::StopWeaponGlow();
			replica->Disable();
		}

		LightningDash::StartCooldown(*player);
		LightningDash::Begin(*player, weaponState.GetActiveWeapon(), destination, slamGround, [this]() {
			// Si volvió sola entretanto (agua, inmune o tiempo máximo), el regreso ya la trae a la mano.
			const auto current = weaponState.GetState();
			if (current == State::kThrown || current == State::kStuck) {
				RecallWeapon();
			}
		});
	}

	void WeaponManager::OnLoadingScreenClosed()
	{
		// Un Lightning Dash en curso se corta.
		LightningDash::Cancel();

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
			// En kThrowing basta cortar el gesto y volver a mano.
			CloseGestureClip(Animation::Gesture::kThrow, false);
			TransitionState(State::kInHand);
			break;
		case State::kCalling:
			// En kCalling el arma sigue fuera: se corta el gesto y RecallWeapon.
			CloseGestureClip(Animation::Gesture::kCall, false);
			RecallWeapon();
			break;
		default:
			break;
		}

		// Gesto de Atrape interrumpido (RecallWeapon ya reequipó y apaga las chispas).
		if (catchState.active) {
			catchState = {};
			CloseGestureClip(Animation::Gesture::kCatch, false);
		}

		// Igual para la cola de Llamada.
		if (callAnimationActive) {
			callAnimationActive = false;
			CloseGestureClip(Animation::Gesture::kCall, false);
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

		// Antes del desequipado de la cola: el vigilante tiene que ver el final del desenvainado que provoca.
		Events::GraphSettleWatcher::Track(*player);

		StartGestureClip(*player, Animation::Gesture::kThrow);

		// Red de seguridad: el lanzamiento ocurre aunque no llegue la anotación.
		ScheduleReleaseFallback(
			Constants::kThrowReleaseFallbackWindow, "Throw.hkx",
			[this] { return weaponState.GetState() == State::kThrowing; },
			[this] { OnThrowReleaseAnimationEvent(); });
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

		Animation::SetTrigger(Animation::Gesture::kThrow, false);

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
		callAnimationFirstPerson = ActorUtils::IsPlayerInFirstPerson();

		StartGestureClip(*player, Animation::Gesture::kCall);

		// Red de seguridad: el regreso empieza aunque no llegue la anotación.
		ScheduleReleaseFallback(
			Constants::kCallReleaseFallbackWindow, "Call.hkx",
			[this] { return weaponState.GetState() == State::kCalling; },
			[this] { OnCallReleaseAnimationEvent(); });
	}

	void WeaponManager::OnCallReleaseAnimationEvent()
	{
		if (weaponState.GetState() != State::kCalling) {
			return;
		}

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			// Chasquido en el mismo instante que el regreso.
			Audio::PlayFileOneShot(player->GetPosition(), Constants::kCallReleaseSoundFilePath, Constants::kCallReleaseSoundVolume);
		}

		// El resto del gesto se difiere a FinishCallAnimation.
		BeginReturn(wasStuckBeforeCalling);

		// FinishCallAnimation ya comprueba callAnimationActive.
		const auto tail = callAnimationFirstPerson ? Constants::kCallAnimationTailDurationFirstPerson : Constants::kCallAnimationTailDuration;
		(void)Scheduler::After(tail, [this]() {
			FinishCallAnimation();
		});
	}

	void WeaponManager::FinishCallAnimation()
	{
		if (!callAnimationActive) {
			// Ya limpiado por otra vía.
			return;
		}
		callAnimationActive = false;

		// Bandera y timestamp juntos, para que una pulsación no lea un timestamp viejo.
		lastAttackAnimationEventTime = std::chrono::steady_clock::now();

		CloseGestureClip(Animation::Gesture::kCall, true);
	}

	void WeaponManager::BeginCatchAnimation()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			// Sin jugador: recuperación instantánea.
			RecallWeapon();
			return;
		}
		if (catchState.active) {
			// Evita disparar el gesto dos veces.
			return;
		}

		// No toca weaponState: el gesto se sigue con catchState. La llegada física puede haberse anotado ya
		// (physicallyArrived, reequipPending): no se reinicia aquí sino en BeginReturn.
		catchState.active = true;
		catchState.reequipDone = false;
		catchState.endSoundPlayed = false;
		catchAnimationFirstPerson = ActorUtils::IsPlayerInFirstPerson();

		// Brillo de manos.
		Animation::TriggerHandGlow(*player);

		if (StartGestureClip(*player, Animation::Gesture::kCatch)) {
			// Catch.hkx empieza ahora: el arma debe llegar a su anotación; se mide cuánto tarda.
			catchAnimationStartTime = FrameHook::Now();
			catchLeadMeasurePending = true;
			if (catchSync) {
				catchSync->OnCatchStarted();
			}
		} else if (catchSync) {
			catchSync->Release();
		}

		// Red de seguridad: el reequipado ocurre aunque no llegue la anotación
		// (kCatchReleaseFallbackWindow > kCatchAnimationLeadTime).
		ScheduleReleaseFallback(
			Constants::kCatchReleaseFallbackWindow, "Catch.hkx",
			[this] { return catchState.active; },
			[this] { OnCatchReleaseAnimationEvent(false); });
	}

	void WeaponManager::OnCatchReleaseAnimationEvent(bool a_fromAnnotation)
	{
		if (!catchState.active || catchState.reequipDone) {
			return;
		}

		// Tiempo de Catch.hkx hasta su anotación (reloj FrameHook::Now), para fijar la llegada del próximo Atrape
		// con la misma cámara.
		if (a_fromAnnotation && catchLeadMeasurePending) {
			catchLeadMeasurePending = false;
			const float measured = static_cast<float>(FrameHook::Now() - catchAnimationStartTime);
			if (!catchLeadTime.Record(catchAnimationFirstPerson, measured)) {
				logs::warn("WeaponManager: medida de Catch.hkx fuera de rango ({:.3f} s), se conserva {:.3f} s.", measured, catchLeadTime.Get(catchAnimationFirstPerson));
			}
		}

		// Sonido final en el instante de la anotación, antes de esperar la llegada física;
		// catchState.endSoundPlayed evita repetirlo.
		if (!catchState.endSoundPlayed) {
			catchState.endSoundPlayed = true;
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				Audio::CatchCue::PlayEnd(player->GetPosition());
			}
		}

		// Si la réplica aún no llegó, el reequipado espera a OnPhysicalArrival.
		if (!catchState.physicallyArrived) {
			catchState.reequipPending = true;
			return;
		}

		PerformCatchReequip();
	}

	void WeaponManager::OnPhysicalArrival()
	{
		catchState.physicallyArrived = true;
		if (catchState.reequipPending) {
			catchState.reequipPending = false;
			PerformCatchReequip();
		}
	}

	void WeaponManager::PerformCatchReequip()
	{
		catchState.reequipDone = true;

		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			// Temblor de cámara al cerrar la mano, con epicentro en el jugador.
			RE::ShakeCamera(Constants::kCatchShakeStrength, player->GetPosition(), Constants::kCatchShakeDuration);
		}

		// Reequipa con la anotación de Catch.hkx y las chispas pasan a seguir la mano.
		// iRightHandType no se toca: ya vale "una mano".
		ReequipAndReset(true);

		// El resto del gesto se difiere a FinishCatchAnimation (cola del clip de la cámara con la que arrancó).
		const auto tail = catchAnimationFirstPerson ? Constants::kCatchAnimationTailDurationFirstPerson : Constants::kCatchAnimationTailDuration;
		(void)Scheduler::After(tail, [this]() {
			FinishCatchAnimation();
		});
	}

	void WeaponManager::FinishCatchAnimation()
	{
		if (!catchState.active) {
			// Ya limpiado por otra vía.
			return;
		}
		catchState = {};

		// Bandera y timestamp juntos, para que una pulsación no lea un timestamp viejo.
		lastAttackAnimationEventTime = std::chrono::steady_clock::now();

		CloseGestureClip(Animation::Gesture::kCatch, true);

		// Apaga las chispas al final del Atrape, tras kCatchVfxSettleDelay.
		Animation::FadeOutMovementVFX(true);
		Animation::StopWeaponGlow();
	}

	void WeaponManager::ThrowWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();

		if (player && weapon) {
			// Se oculta el arma (también de los menús) y la réplica toma el relevo; el desequipado real se difiere.
			Animation::SetEquippedWeaponHidden(*player, true);
			SetHiddenFromMenus(weapon, true);
			throwTailActive = true;
			lastAttackAnimationEventTime = std::chrono::steady_clock::now();

			// Desequipado real pasado kThrowReleaseVisualHoldDuration (cancelable con throwTailToken).
			throwTailToken = Scheduler::After(Constants::kThrowReleaseVisualHoldDuration, [this, player, weapon]() {
				throwTailActive = false;

				// Con las manos vacías el motor desenvaina los puños; sus eventos llegan unos fotogramas después.
				Events::GraphSettleWatcher::NoteDrawExpected();
				ActorUtils::UnequipNow(*player, weapon);

				// Solo desbloquea movimiento y AnimationDriven si Llamada o Atrape no los han tomado.
				if (!callAnimationActive && !catchState.active) {
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
			callbacks.onStuck = [this](RE::ActorHandle a_actor, const RE::NiPoint3& a_surfaceNormal) {
				// Solo si el ciclo sigue en kThrown.
				if (weaponState.GetState() == State::kThrown) {
					weaponState.SetStuckActorHandle(a_actor);
					weaponState.SetStuckSurfaceNormal(a_surfaceNormal);

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
		// Libera al objetivo y quita el hazard al iniciar el regreso.
		ReleaseStuckTarget();

		// Cancela el bucle que movía la réplica antes de arrancar el del regreso.
		weaponState.CancelTickLoop();

		auto* player = RE::PlayerCharacter::GetSingleton();
		auto  replicaHandle = weaponState.GetActiveReplicaHandle();

		// Devuelve la colisión a la réplica.
		Combat::RestoreReplicaCollision(replicaHandle.get().get());

		if (!player || !replicaHandle.get()) {
			logs::warn("WeaponManager::BeginReturn: sin jugador o réplica válida, recuperación instantánea de reserva.");
			RecallWeapon();
			return;
		}

		TransitionState(State::kReturning);

		// Se reinician en cada regreso.
		catchState.physicallyArrived = false;
		catchState.reequipPending = false;

		Return::ReturnCallbacks callbacks;
		callbacks.onTickStarted = [this](Physics::TickToken a_token) {
			weaponState.SetActiveTickToken(a_token);
		};
		callbacks.onApproaching = [this]() {
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

		// Llegada a la anotación del Atrape de la cámara activa.
		catchSync = std::make_shared<Return::CatchSync>(catchLeadTime.Get(ActorUtils::IsPlayerInFirstPerson()));
		Return::BeginReturn(player, replicaHandle, a_wasStuck, catchSync, std::move(callbacks));
	}

	void WeaponManager::RecallWeapon()
	{
		ReleaseStuckTarget();
		// Olvida la capa guardada.
		Combat::RestoreReplicaCollision(nullptr);

		// Sin animación: fundido inmediato.
		ReequipAndReset();
		Animation::FadeOutMovementVFX();
		Animation::StopWeaponGlow();
	}

	void WeaponManager::ReequipAndReset(bool a_reattachVfxToHand)
	{
		// Cancela el desequipado diferido de Lanzar si seguía pendiente: el arma sigue equipada, solo oculta.
		const bool throwTailWasPending = throwTailActive;
		Scheduler::Cancel(throwTailToken);
		throwTailActive = false;

		// Las chispas no se apagan aquí; con a_reattachVfxToHand pasan a seguir la mano.
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (a_reattachVfxToHand && player) {
			Animation::RetargetMovementVFXToActor(*player);
			Animation::RetargetWeaponGlowToActor(*player);
		}

		weaponState.CancelTickLoop();
		catchSync.reset();

		Physics::DestroyReference(weaponState.GetActiveReplicaHandle());
		weaponState.SetActiveReplicaHandle({});

		// De vuelta en los menús antes de reequiparla.
		auto* weapon = weaponState.GetActiveWeapon();
		SetHiddenFromMenus(weapon, false);

		if (player && weapon && throwTailWasPending) {
			// Sin desequipar todavía: se vuelve a mostrar en vez de reequiparla encima (retiraría Lightning Dash) y se hace la
			// limpieza del desequipado diferido; el ataque de Throw.hkx, que cerraba el desequipado, lo cierra attackStop.
			Animation::SetEquippedWeaponHidden(*player, false);
			if (!callAnimationActive && !catchState.active) {
				Animation::SetAnimationDriven(*player, false);
				Input::SetMovementLocked(false);
				Events::GraphSettleWatcher::NoteAttackStopSent(true);
				if (!player->NotifyAnimationGraph(Constants::kAttackStopAnimationEvent)) {
					Events::GraphSettleWatcher::NoteAttackStopSent(false);
				}
			}
		} else if (player && weapon) {
			// Diferido un tick: tras una pantalla de carga, síncrono no equipa.
			SKSE::GetTaskInterface()->AddTask([this, player, weapon]() {
				// Sin animación de equipar/desenvainar (mod SkipEquipAnimation).
				player->SetGraphVariableBool(Constants::kSkipEquipAnimationGraphVariable, true);
				if (!ActorUtils::EquipNow(*player, weapon)) {
					// Se lo quitó un script mientras estaba fuera (p. ej. al confiscarlo): la réplica ya no está, y sin arma no hay poder.
					logs::warn("WeaponManager::ReequipAndReset: el arma ya no está en el inventario, no se reequipa.");
					OnThrowableWeaponEquipChanged(false);
				}

				// Se apaga pasado kSkipEquipAnimationWindow; cancela el temporizador anterior.
				Scheduler::Cancel(skipEquipAnimationToken);
				skipEquipAnimationToken = Scheduler::After(Constants::kSkipEquipAnimationWindow, [player]() {
					player->SetGraphVariableBool(Constants::kSkipEquipAnimationGraphVariable, false);
				});
			});
		}

		weaponState.SetActiveWeapon(nullptr);
		TransitionState(State::kInHand, false);
	}

	bool WeaponManager::StartGestureClip(RE::PlayerCharacter& a_player, Animation::Gesture a_gesture)
	{
		Input::SetMovementLocked(true);
		Animation::SetAnimationDriven(a_player, true);

		// Llamada y Atrape: el arma real no está en la mano; iRightHandType al tipo del arma lanzada para que la rama
		// de combate reproduzca el clip.
		if (a_gesture != Animation::Gesture::kThrow) {
			a_player.SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, Animation::GetRightHandTypeFor(weaponState.GetActiveWeapon()));
		}

		// El Global hace que OAR sustituya el ataque ligero por el clip del gesto.
		Animation::SetTrigger(a_gesture, true);

		// Si el grafo rechaza el evento, el clip no se verá (conflicto con otro behavior).
		const bool accepted = a_player.NotifyAnimationGraph(Constants::kLightAttackAnimationEvent);
		if (!accepted) {
			logs::warn("WeaponManager: el grafo de animación rechazó '{}' para {}; el gesto seguirá por la red de seguridad.", Constants::kLightAttackAnimationEvent, GestureName(a_gesture));
		}
		return accepted;
	}

	void WeaponManager::CloseGestureClip(Animation::Gesture a_gesture, bool a_sendAttackStop)
	{
		Animation::SetTrigger(a_gesture, false);
		if (auto* player = RE::PlayerCharacter::GetSingleton()) {
			Animation::SetAnimationDriven(*player, false);

			// Llamada vuelve iRightHandType a 0 (desarmado); en el Atrape ya vale el del arma reequipada.
			if (a_gesture == Animation::Gesture::kCall) {
				player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, 0);
			}

			// attackStop pasada la cola del clip para desatascar el grafo: el submod solo procesa sus anotaciones.
			if (a_sendAttackStop) {
				player->NotifyAnimationGraph(Constants::kAttackStopAnimationEvent);
			}
		}
		Input::SetMovementLocked(false);
	}

	void WeaponManager::ReleaseStuckTarget()
	{
		if (auto actor = weaponState.GetStuckActorHandle().get()) {
			Combat::EndEmbeddedEffect(actor.get());
		}
		weaponState.SetStuckActorHandle({});
		Combat::RemoveImpactHazard();
	}
}
