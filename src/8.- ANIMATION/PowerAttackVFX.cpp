// Efectos visuales durante los power attacks -- ver PowerAttackVFX.h.

#include "8.- ANIMATION/PowerAttackVFX.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "1.- CORE/Settings.h"
#include "3.- WEAPON/WeaponManager.h"
#include "8.- ANIMATION/WeaponGlow.h"
#include "8.- ANIMATION/WeaponVFX.h"

namespace Animation::PowerAttackVFX
{
	namespace
	{
		// Estado de los efectos del power attack en curso. Solo se toca en
		// el hilo principal (el sink reencola con AddTask), sin mutex.
		// g_ownsGlow: el destello lo arrancó este módulo (StartWeaponGlow
		// no arranca uno si ya había otro activo, p. ej. el del final de
		// un Atrape) -- así el apagado nunca corta un destello ajeno.
		bool                   g_active{ false };
		bool                   g_ownsGlow{ false };
		Scheduler::CancelToken g_safetyToken;

		bool HasThrowableWeaponInHand(RE::Actor& a_actor)
		{
			// false = mano derecha / mano principal.
			auto* rightHand = a_actor.GetEquippedObject(false);
			auto* weapon = rightHand ? rightHand->As<RE::TESObjectWEAP>() : nullptr;
			return weapon && weapon->HasKeywordString(Constants::kThrowableWeaponKeyword);
		}

		void ClearState()
		{
			Scheduler::Cancel(g_safetyToken);
			g_safetyToken.reset();
			g_active = false;
			g_ownsGlow = false;
		}

		// Apagado normal: los dos efectos con su propio fundido.
		void Stop(std::string_view a_reason)
		{
			if (!g_active) {
				return;
			}

			logs::info("PowerAttackVFX: apagando efectos ({}).", a_reason);
			const bool ownsGlow = g_ownsGlow;
			ClearState();

			// Si el ciclo ya no está en reposo, los efectos activos son del
			// lanzamiento (Cancel ya debería haber limpiado el estado antes
			// de llegar aquí, esto es solo una red por si acaso).
			if (Weapon::WeaponManager::GetSingleton()->GetState() != Weapon::State::kInHand) {
				return;
			}

			Animation::FadeOutMovementVFX();
			if (ownsGlow) {
				Animation::StopWeaponGlow();
			}
		}

		void OnSwing()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}

			// Un ataque normal encadenado tras un power attack (combo) no
			// pasa por attackStop entre medias: se apaga aquí.
			if (!player->IsPowerAttacking()) {
				Stop("swing sin power attack");
				return;
			}

			if (!Settings::GetPowerAttackEffects() ||
				Weapon::WeaponManager::GetSingleton()->GetState() != Weapon::State::kInHand ||
				!HasThrowableWeaponInHand(*player)) {
				return;
			}

			// Power attacks encadenados, o el mismo evento llegando desde
			// los dos grafos (tercera y primera persona): los efectos ya
			// están encendidos, solo se alarga la red de seguridad.
			if (!g_active) {
				logs::info("PowerAttackVFX: power attack, encendiendo efectos.");
				g_active = true;
				Animation::StartMovementVFXOnActor(*player, false);
				g_ownsGlow = Animation::StartWeaponGlow(*player, false);
			}

			Scheduler::Cancel(g_safetyToken);
			g_safetyToken = Scheduler::After(Constants::kPowerAttackVfxSafetyTimeout, [] {
				Stop("red de seguridad, sin attackStop");
			});
		}

		class Sink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!a_event || !player || a_event->holder != player) {
					return RE::BSEventNotifyControl::kContinue;
				}

				// Hilo de animación: todo lo que toca efectos/3D se reencola
				// al hilo principal.
				const std::string_view tag = a_event->tag.c_str();
				if (tag == Constants::kPowerAttackVfxStartEvent) {
					SKSE::GetTaskInterface()->AddTask(OnSwing);
				} else if (tag == Constants::kPowerAttackVfxStopEvent) {
					SKSE::GetTaskInterface()->AddTask([] { Stop("attackStop"); });
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		Sink sink;
	}

	void EnsureRegistered(RE::Actor& a_actor)
	{
		// Todos los grafos, igual que AttackAnimType::EnsureRegistered (el
		// jugador tiene uno de tercera y otro de primera persona).
		// BSTEventSource::AddEventSink ya ignora un sink repetido.
		RE::BSAnimationGraphManagerPtr graphManager;
		a_actor.GetAnimationGraphManager(graphManager);
		if (!graphManager) {
			logs::warn("PowerAttackVFX::EnsureRegistered: '{}' sin grafo de animación todavía.", a_actor.GetName());
			return;
		}

		for (const auto& graph : graphManager->graphs) {
			if (graph) {
				graph->GetEventSource<RE::BSAnimationGraphEvent>()->AddEventSink(&sink);
			}
		}
	}

	void Cancel()
	{
		if (!g_active) {
			return;
		}

		logs::info("PowerAttackVFX: cancelado por el inicio de un lanzamiento.");
		const bool ownsGlow = g_ownsGlow;
		ClearState();

		if (ownsGlow) {
			Animation::StopWeaponGlow();
		}

		// Con las chispas del lanzamiento desactivadas nadie va a relevar
		// las de este power attack: se cortan aquí.
		if (!Settings::GetParticles()) {
			Animation::StopMovementVFX();
		}
	}
}
