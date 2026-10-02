// Ciclo del arma: lanzar, clavarse, llamar, regresar y atrapar; coordina Throw, Return, Combat,
// animaciones (OAR), VFX y equipado real. Recibe la entrada, los eventos y los avisos de OAR.

#pragma once

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "3.- WEAPON/WeaponState.h"

#include <chrono>
#include <memory>
#include <vector>

namespace Return
{
	class CatchSync;
}

namespace Weapon
{
	class WeaponManager
	{
	public:
		// Datos del ciclo guardados en el cosave (FormID, remapeados al cargar).
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

		// Pulsar/soltar la tecla de acción. Los llama InputManager.
		void OnActionButtonDown();
		void OnActionButtonUp();

		[[nodiscard]] State GetState() const noexcept { return weaponState.GetState(); }

		// Arma del ciclo actual (nullptr en mano).
		[[nodiscard]] RE::TESBoundObject* GetActiveWeapon() const noexcept { return weaponState.GetActiveWeapon(); }

		// Réplica del ciclo actual. La usa GlowMapControl.
		[[nodiscard]] RE::ObjectRefHandle GetActiveReplicaHandle() const noexcept { return weaponState.GetActiveReplicaHandle(); }

		// Vuelve a "en mano" sin tocar el arma física. Lo llama EventManager en kNewGame.
		void ResetToInHand();

		// Datos del ciclo a guardar. Lo llama el callback de guardado del cosave.
		[[nodiscard]] SaveCycleData CaptureSaveData() const;

		// Al cargar partida: si se guardó a mitad de ciclo, libera al actor, borra la réplica
		// y reequipa; si no, como ResetToInHand. Lo llama EventManager en kPostLoadGame.
		void RecoverOrReset(const SaveCycleData& a_data);

		// Con el ciclo en marcha, recupera el arma al instante. Lo llama EventManager tras la pantalla de carga.
		void OnLoadingScreenClosed();

		// Anotación de liberación de Throw.hkx: lanza el arma. La llaman OARFunctions
		// y la red de seguridad (Constants::kThrowReleaseFallbackWindow).
		void OnThrowReleaseAnimationEvent();

		// Anotación de liberación de Call.hkx: chasquido y Return::BeginReturn. La llaman OARFunctions
		// y la red de seguridad (Constants::kCallReleaseFallbackWindow).
		void OnCallReleaseAnimationEvent();

		// Pasada la cola de Call.hkx, envía attackStop y suelta bloqueos y el trigger de Llamada.
		void FinishCallAnimation();

		// Anotación de Catch.hkx (mano cerrada): sonido final y reequipado, diferido hasta la llegada física
		// si aún no llegó. La llaman OARFunctions (a_fromAnnotation=true) y la red de seguridad (false).
		void OnCatchReleaseAnimationEvent(bool a_fromAnnotation);

		// Llegada física de la réplica a la mano (onArrived de Return); completa un reequipado pendiente.
		void OnPhysicalArrival();

		// Reequipado del Atrape: temblor de cámara, ReequipAndReset y espera de la cola del clip.
		void PerformCatchReequip();

		// Pasada la cola de Catch.hkx, envía attackStop y suelta bloqueos y el trigger de Atrape.
		void FinishCatchAnimation();

		// true mientras se equipa el arma señuelo del gesto. Lo consulta EquipGuard.
		[[nodiscard]] bool IsEquipGuardSuppressed() const noexcept { return suppressEquipGuard; }

		// Concede el poder Lightning Dash al equiparla y lo retira al desequiparla en reposo.
		// Lo llama LightningDashWatcher (EventManager).
		void OnThrowableWeaponEquipChanged(bool a_equipped);

		// Al cargar partida, concede el poder si el arma ya está en la mano (nunca lo retira).
		void RestoreLightningDashPower();

	private:
		WeaponManager() = default;
		~WeaponManager() = default;

		// Cambia de estado y mueve las chispas al objetivo del nuevo estado (mano, réplica o ninguno).
		// Con a_manageVfx=false deja las chispas como están.
		void TransitionState(State a_newState, bool a_manageVfx = true);

		// Comprueba que se puede lanzar y fija el arma activa. false si no se puede.
		[[nodiscard]] bool PrepareThrow();

		// Pasa a kThrowing: activa el Global de OAR y dispara el ataque que reproduce Throw.hkx.
		void BeginThrowAnimation();

		// Si hay un ataque en curso lo corta y ejecuta a_action cuando el grafo vuelve a reposo;
		// si no, al instante. a_action debe revisar el estado.
		void InterruptAttackThen(std::function<void()> a_action);

		// Corta un bloqueo en curso y ejecuta a_action pasado kBlockInterruptSettleDelay.
		void InterruptBlockThen(RE::PlayerCharacter& a_player, std::function<void()> a_action);

		// Desequipa el arma, pasa a kThrown y lanza la réplica con Throw::LaunchWeapon.
		// La llama OnThrowReleaseAnimationEvent.
		void ThrowWeapon();

		// Pasa a kCalling: iRightHandType a una mano, Global de OAR de Llamada y ataque que reproduce Call.hkx.
		// Guarda si el arma estaba clavada para BeginReturn.
		void BeginCallAnimation();

		// Sin uso: equipa el arma real como señuelo para el gesto (reserva).
		void EquipGestureWeapon();

		// Sin uso: desequipa el arma señuelo.
		void UnequipGestureWeapon();

		// Cancela el bucle actual, libera al objetivo y arranca Return::BeginReturn sobre la réplica.
		// Sin jugador o réplica, RecallWeapon.
		void BeginReturn(bool a_wasStuck);

		// Gesto de Atrape (Catch.hkx); al empezar fija en catchSync la llegada del arma. No cambia el estado.
		// Lo llama onApproaching de Return.
		void BeginCatchAnimation();

		// Recuperación instantánea: borra la réplica y reequipa el arma.
		void RecallWeapon();

		// Borra la réplica, cancela el bucle y reequipa el arma sin animación (SkipEquipAnimation).
		// Con a_reattachVfxToHand, las chispas pasan a seguir la mano (Atrape animado).
		void ReequipAndReset(bool a_reattachVfxToHand = false);

		WeaponState weaponState;

		// Si el arma estaba clavada al llamar, para BeginReturn.
		bool wasStuckBeforeCalling{ false };

		// Activo mientras se equipa el señuelo.
		bool suppressEquipGuard{ false };

		// true desde que ThrowWeapon oculta el arma hasta que termina su desequipado diferido.
		bool throwTailActive{ false };

		// Token del desequipado diferido de ThrowWeapon; lo cancela ReequipAndReset.
		Scheduler::CancelToken throwTailToken;

		// Token del apagado diferido de "SkipEquipAnimation"; ReequipAndReset cancela el anterior.
		Scheduler::CancelToken skipEquipAnimationToken;

		// true mientras InterruptAttackThen espera; se ignoran pulsaciones nuevas.
		bool                   attackInterruptActive{ false };
		Scheduler::CancelToken attackInterruptToken;

		// true si la pulsación empezó en reposo con el arma desenvainada: solo entonces el suelte lanza.
		bool throwPressArmed{ false };

		// Instante del último cambio del grafo por nuestra cuenta, para respetar kMinAttackStartInterval.
		std::chrono::steady_clock::time_point lastAttackAnimationEventTime;

		// true desde BeginCatchAnimation hasta FinishCatchAnimation.
		bool catchAnimationActive{ false };

		// true desde el reequipado del Atrape hasta FinishCatchAnimation (evita repetirlo).
		bool catchReequipDone{ false };

		// true cuando la réplica llega físicamente a la mano. Se reinicia en cada regreso.
		bool catchPhysicallyArrived{ false };

		// true si el reequipado esperó a la llegada física; lo completa OnPhysicalArrival.
		bool catchReequipPending{ false };

		// true tras el sonido final del atrape, para no repetirlo si llegan la anotación y la red de seguridad.
		bool catchEndSoundPlayed{ false };

		// Sincronía del regreso en curso con Catch.hkx; se crea en BeginReturn.
		std::shared_ptr<Return::CatchSync> catchSync;

		// Tiempo de Catch.hkx hasta su anotación (reloj FrameHook::Now): mediana de catchLeadSamples (nominal al cargar).
		float catchLeadSeconds{ Constants::kCatchAnimationLeadTime };

		// Últimas medidas válidas de Catch.hkx (hasta Constants::kCatchLeadSampleCount), la más antigua primero.
		std::vector<float> catchLeadSamples;

		// Instante (FrameHook::Now) del attackStart de Catch.hkx y si falta medir su anotación.
		double catchAnimationStartTime{ 0.0 };
		bool   catchLeadMeasurePending{ false };

		// true desde BeginCallAnimation hasta FinishCallAnimation.
		bool callAnimationActive{ false };
	};
}
