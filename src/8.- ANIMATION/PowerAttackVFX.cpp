// Efectos visuales durante los power attacks -- ver PowerAttackVFX.h.

#include "8.- ANIMATION/PowerAttackVFX.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "1.- CORE/Settings.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "3.- WEAPON/WeaponManager.h"
#include "8.- ANIMATION/WeaponGlow.h"
#include "8.- ANIMATION/WeaponVFX.h"

namespace Animation::PowerAttackVFX
{
	namespace
	{
		// Estado del power attack en curso (solo hilo principal).
		// g_ownsGlow: el destello lo encendió este módulo, así solo apaga el suyo.
		bool                   g_active{ false };
		bool                   g_ownsGlow{ false };
		Scheduler::CancelToken g_safetyToken;

		void ClearState()
		{
			Scheduler::Cancel(g_safetyToken);
			g_active = false;
			g_ownsGlow = false;
		}

		// Apaga los dos efectos con su fundido.
		void Stop()
		{
			if (!g_active) {
				return;
			}

			const bool ownsGlow = g_ownsGlow;
			ClearState();

			// Fuera de reposo los efectos activos son del lanzamiento: no se tocan.
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

			// Un ataque normal encadenado tras un power attack los apaga.
			if (!player->IsPowerAttacking()) {
				Stop();
				return;
			}

			if (!Settings::GetPowerAttackEffects() ||
				Weapon::WeaponManager::GetSingleton()->GetState() != Weapon::State::kInHand ||
				!ActorUtils::IsThrowableWeaponEquipped(player)) {
				return;
			}

			// Ya encendidos (combo o evento repetido desde el otro grafo): solo alarga la red.
			if (!g_active) {
				g_active = true;
				Animation::StartMovementVFXOnActor(*player, false);
				g_ownsGlow = Animation::StartWeaponGlow(*player, false);
			}

			Scheduler::Cancel(g_safetyToken);
			g_safetyToken = Scheduler::After(Constants::kPowerAttackVfxSafetyTimeout, [] {
				Stop();
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

				// Hilo de animación: el trabajo se reencola al hilo principal.
				const std::string_view tag = a_event->tag.c_str();
				if (tag == Constants::kPowerAttackVfxStartEvent) {
					SKSE::GetTaskInterface()->AddTask(OnSwing);
				} else if (tag == Constants::kAttackStopAnimationEvent) {
					SKSE::GetTaskInterface()->AddTask([] { Stop(); });
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		Sink sink;
	}

	void EnsureRegistered(RE::Actor& a_actor)
	{
		// Sin grafo todavía (p. ej. la intro de una partida nueva) no engancha nada: se reintenta al equipar el arma.
		(void)ActorUtils::AddEventSinkToAllGraphs(a_actor, &sink);
	}

	void Cancel()
	{
		if (!g_active) {
			return;
		}

		const bool ownsGlow = g_ownsGlow;
		ClearState();

		if (ownsGlow) {
			Animation::StopWeaponGlow();
		}

		// Sin chispas de lanzamiento nadie releva estas: se cortan aquí.
		if (!Settings::GetParticles()) {
			Animation::StopMovementVFX();
		}
	}
}
