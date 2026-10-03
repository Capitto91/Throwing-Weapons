// Poder Lightning Dash: cooldown, desplazamiento del jugador hasta el arma (sin colisión, con la
// animación del grito de sprint) y retirada de los efectos persistentes de su VisualEffect. Lo usa WeaponManager.

#pragma once

#include <functional>

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

	// Desplaza a a_player hasta a_destination a Constants::kLightningDashSpeed atravesando obstáculos y
	// llama a a_onArrived (fuera del bucle de tick) al llegar, no si se cancela. Lo llama WeaponManager::OnLightningDashCast.
	void Begin(RE::PlayerCharacter& a_player, const RE::NiPoint3& a_destination, std::function<void()> a_onArrived);

	// Corta el desplazamiento en curso sin llamar a a_onArrived. Lo llama WeaponManager al reiniciar el ciclo.
	void Cancel();

	// true desde Begin hasta la llegada o Cancel.
	[[nodiscard]] bool IsActive() noexcept;

	// Retira del jugador los efectos con el arte o el shader de CAP_ThorMjolnir_VisualEffect_LightningDash
	// (Constants::kLightningDashVisualEffectLocalFormID). Lo llama EventManager al cargar partida y al cerrar la pantalla de carga.
	void RemoveLegacyEffects();
}
