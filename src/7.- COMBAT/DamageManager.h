// Gestiona la aplicación de daño provocado por el arma.
// Se utiliza tanto durante el lanzamiento como durante el retorno.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

#include <functional>

namespace Combat
{
	// Resuelve la función nativa de procesar golpe
	// (GameOffsets::ResolveProcessHit). Los porcentajes de daño viven en
	// Settings ([Damage] del INI). Debe llamarse una única vez, en
	// kDataLoaded (ver Events::OnSKSEMessage).
	void Init();

	// Punto 6 de Mecanica del arma.txt: la réplica acaba de impactar y
	// detenerse sobre a_target (un actor), en vez de una superficie.
	// Aplica el golpe inicial (daño real del arma x
	// Settings::GetThrowHitMult(), 75% por defecto, por el pipeline de
	// combate del motor) y, si el objetivo puede paralizarse (ni dragón,
	// Actor::IsDragon(), ni inmune según la condición del propio hechizo,
	// comprobado indirectamente vía el estado de aturdimiento tras
	// concederlo), concede la habilidad de parálisis propia
	// (Constants::kEmbeddedParalysisSpell) y arranca un seguimiento
	// continuo: la réplica sigue la posición de a_target tick a tick
	// (aproximación por posición, sin enganche real a hueso — no hay API
	// para localizar el hueso más cercano a un punto, ver CLAUDE.md), y
	// llama a a_onAutoRecall si el objetivo resulta inmune o al superar
	// Constants::kEmbeddedMaxDuration clavada. Si el objetivo puede
	// paralizarse, llama primero a a_onStuck (con el handle del actor)
	// para que el llamante registre el ciclo como "clavada".
	// a_onTickStarted recibe el token del nuevo bucle de seguimiento que
	// arranca aqui (ver Physics::TickToken): sustituye al bucle de vuelo
	// de Throw::LaunchWeapon, y el llamante debe quedarse con este token
	// nuevo para poder cancelarlo mas adelante (p. ej. al pulsar el boton
	// de recuperar).
	void BeginEmbeddedEffect(
		RE::Actor*                              a_attacker,
		RE::Actor*                              a_target,
		RE::ObjectRefHandle                     a_replicaHandle,
		std::function<void(RE::ActorHandle)>    a_onStuck,
		std::function<void()>                   a_onAutoRecall,
		std::function<void(Physics::TickToken)> a_onTickStarted);

	// Daño eléctrico de impacto (a petición del usuario 2026-09-27: en
	// cualquier impacto de la ida, no solo contra actores). Se coloca un
	// tick después del impacto, fuera del callback de tick
	// (PlaceObjectAtMe síncrono ahí crasheó, ver Throw::LaunchWeapon):
	// capturar GetHazardGeneration() en el impacto y pasarlo, para que no
	// se coloque si el arma se desclavó entretanto. Dura su Lifetime (5 s)
	// salvo que se desclave antes (RemoveImpactHazard).
	[[nodiscard]] std::uint32_t GetHazardGeneration();

	// Contra un actor: hazard "Drop" (Constants::kEmbeddedHazardLocalFormID)
	// sobre él, cae al suelo bajo sus pies.
	void SpawnActorHazard(RE::Actor* a_attacker, RE::Actor& a_target, std::uint32_t a_generation);

	// Contra una superficie: hazard sin Drop
	// (Constants::kSurfaceHazardLocalFormID) en a_point, orientado sobre
	// a_normal (pared -> pegado a la pared, suelo -> en el suelo).
	// a_anchor solo sirve para PlaceObjectAtMe (cualquier referencia
	// cargada; se reposiciona justo después).
	void SpawnSurfaceHazard(RE::Actor* a_attacker, RE::TESObjectREFR& a_anchor, const RE::NiPoint3& a_point, const RE::NiPoint3& a_normal, std::uint32_t a_generation);

	// Quita el hazard del impacto actual, si sigue ahí, e invalida uno
	// pendiente de colocar. Llamar al desclavar el arma (inicio del
	// regreso) y al recogerla.
	void RemoveImpactHazard();

	// Libera al objetivo (punto 6): quita la habilidad de parálisis
	// concedida por BeginEmbeddedEffect. Debe llamarse siempre al
	// recuperar el arma mientras siga clavada en un actor (ver
	// WeaponManager::RecallWeapon).
	void EndEmbeddedEffect(RE::Actor* a_target);

	// Devuelve a la réplica la capa de colisión que tenía antes de
	// clavarse (BeginEmbeddedEffect se la quita para no empujar el ragdoll
	// del objetivo paralizado). Sin efecto si no se quitó. Llamar al
	// desclavar, antes de arrancar el regreso.
	void RestoreReplicaCollision(RE::TESObjectREFR* a_replica);

	// Punto 9 de Mecanica del arma.txt: la réplica ha golpeado a
	// a_target durante el regreso (no durante la ida), sin quedarse
	// clavada. Aplica el daño real del arma x Settings::GetReturnHitMult()
	// (25% por defecto) en a_hitPosition, y un stagger propio garantizado
	// vía animation graph (el que calcula el motor se anula, ver
	// ApplyWeaponHit en el .cpp).
	void ApplyReturnHit(RE::Actor* a_attacker, RE::Actor* a_target, const RE::NiPoint3& a_hitPosition);
}
