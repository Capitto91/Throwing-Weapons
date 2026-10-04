// Poder Lightning Dash: cooldown, desplazamiento del jugador hasta el arma (sin colisión, con estela y efectos),
// golpe en salto y retirada de los efectos persistentes de su VisualEffect. Lo usa WeaponManager.

#pragma once

#include <functional>
#include <optional>

namespace Weapon::LightningDash
{
	// Hechizo del poder (Constants::kLightningDashSpellLocalFormID), resuelto una vez.
	// Lo usan WeaponManager (conceder/retirar) y el sink de TESSpellCastEvent de EventManager.
	[[nodiscard]] RE::SpellItem* GetSpell();

	// true si el efecto de cooldown sigue activo en a_actor.
	[[nodiscard]] bool IsOnCooldown(RE::Actor& a_actor);

	// Lanza sobre a_actor el hechizo de cooldown.
	void StartCooldown(RE::Actor& a_actor);

	// Destino junto al arma en a_weaponPoint, sin bajar al suelo: delante de a_stuckActor (del lado del jugador,
	// a la altura de sus pies) si lo hay; si no, separado de la superficie por a_surfaceNormal (nula en vuelo).
	[[nodiscard]] RE::NiPoint3 ComputeDestination(RE::Actor& a_player, const RE::NiPoint3& a_weaponPoint, RE::Actor* a_stuckActor, const RE::NiPoint3& a_surfaceNormal);

	// Suelo bajo a_destination si queda a menos de Constants::kLightningDashSlamMaxHeight (golpe en salto); si no, nullopt.
	// Atraviesa a_replica. Lo consulta WeaponManager::OnLightningDashCast con el arma en vuelo.
	[[nodiscard]] std::optional<RE::NiPoint3> FindSlamGround(RE::Actor& a_player, const RE::NiPoint3& a_destination, RE::TESObjectREFR* a_replica);

	// Desplaza a a_player hasta a_destination a Constants::kLightningDashSpeed atravesando obstáculos y
	// llama a a_onArrived (fuera del bucle de tick) al llegar, no si se cancela. Con a_slamGround, tras a_onArrived
	// encadena el golpe en salto hasta ese suelo. Lo llama WeaponManager::OnLightningDashCast.
	void Begin(RE::PlayerCharacter& a_player, const RE::NiPoint3& a_destination, std::optional<RE::NiPoint3> a_slamGround, std::function<void()> a_onArrived);

	// Anotación OAR.MjolnirSlam del golpe en salto (o su red de seguridad, a_fromAnnotation=false): termina la bajada,
	// explosión de impacto (solo con la anotación) y attackStop pasada la cola del clip. La encola OARFunctions.
	void OnSlamImpactAnimationEvent(bool a_fromAnnotation);

	// Corta el desplazamiento en curso sin llamar a a_onArrived. Lo llama WeaponManager al reiniciar el ciclo.
	void Cancel();

	// true desde Begin hasta la llegada o Cancel.
	[[nodiscard]] bool IsActive() noexcept;

	// Retira del jugador los efectos con el arte o el shader de CAP_ThorMjolnir_VisualEffect_LightningDash
	// (Constants::kLightningDashVisualEffectLocalFormID). Lo llama EventManager al cargar partida y al cerrar la pantalla de carga.
	void RemoveLegacyEffects();
}
