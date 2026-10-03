// Daño, parálisis, descarga eléctrica y tambaleo que causa el arma en la ida y en el regreso.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

#include <functional>

namespace Combat
{
	// Resuelve la función nativa de procesar golpe. Lo llama EventManager en kDataLoaded.
	void Init();

	// Golpe inicial sobre a_target al clavarse, parálisis y seguimiento de la réplica sobre su hueso.
	// Avisa por a_onStuck, a_onAutoRecall (inmune o tiempo máximo) y a_onTickStarted (token del bucle).
	void BeginEmbeddedEffect(
		RE::Actor*                                                a_attacker,
		RE::Actor*                                                a_target,
		RE::ObjectRefHandle                                       a_replicaHandle,
		std::function<void(RE::ActorHandle, const RE::NiPoint3&)> a_onStuck,
		std::function<void()>                                     a_onAutoRecall,
		std::function<void(Physics::TickToken)>                   a_onTickStarted);

	// Descarga eléctrica (BGSHazard) de un impacto de la ida, colocada un tick después.
	// No se coloca si cambió GetHazardGeneration(); RemoveImpactHazard la quita.
	[[nodiscard]] std::uint32_t GetHazardGeneration();

	// Contra un actor: hazard "Drop" que cae bajo sus pies.
	void SpawnActorHazard(RE::Actor* a_attacker, RE::Actor& a_target, std::uint32_t a_generation);

	// Contra una superficie: hazard en a_point orientado por a_normal.
	// a_anchor solo sirve de referencia para PlaceObjectAtMe.
	void SpawnSurfaceHazard(RE::Actor* a_attacker, RE::TESObjectREFR& a_anchor, const RE::NiPoint3& a_point, const RE::NiPoint3& a_normal, std::uint32_t a_generation);

	// Quita el hazard actual e invalida uno pendiente. Al desclavar y al recoger el arma.
	void RemoveImpactHazard();

	// Quita la parálisis de BeginEmbeddedEffect. Lo llama WeaponManager al recuperar el arma clavada.
	void EndEmbeddedEffect(RE::Actor* a_target);

	// Devuelve a la réplica su capa de colisión original. Al desclavar, antes del regreso.
	void RestoreReplicaCollision(RE::TESObjectREFR* a_replica);

	// Golpe del regreso sobre a_target en a_hitPosition, con tambaleo propio.
	// Lo llama Return en cada actor que atraviesa.
	void ApplyReturnHit(RE::Actor* a_attacker, RE::Actor* a_target, const RE::NiPoint3& a_hitPosition);
}
