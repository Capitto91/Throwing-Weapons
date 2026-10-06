// Daño y efectos de impacto -- ver DamageManager.h.

#include "7.- COMBAT/DamageManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Forms.h"
#include "1.- CORE/GameOffsets.h"
#include "1.- CORE/Settings.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "3.- WEAPON/WeaponManager.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <cmath>
#include <optional>

namespace Combat
{
	namespace
	{
		// Función nativa de procesar golpe, resuelta en Init (nullptr en VR: se usa el respaldo).
		GameOffsets::tProcessHit g_processHit = nullptr;

		void ApplyDamage(RE::Actor* a_target, float a_amount)
		{
			// DamageActorValue vía AsActorValueOwner(): el offset de la base cambia entre versiones.
			auto* avOwner = a_target->AsActorValueOwner();
			if (avOwner) {
				avOwner->DamageActorValue(RE::ActorValue::kHealth, a_amount);
			}
		}

		// Respaldo sin g_processHit: daño directo y CombatHit solo para avisar a la IA (revirtiendo su daño).
		void NotifyHit(RE::Actor* a_attacker, RE::Actor* a_target, float a_amount)
		{
			auto* avOwner = a_target->AsActorValueOwner();

			if (REL::Module::IsVR()) {
				a_target->SetBeenAttacked(true);
				a_target->HandleHealthDamage(a_attacker, a_amount);
				return;
			}

			const float before = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;

			GameOffsets::DealDamage(a_attacker, a_target, nullptr, false);

			const float afterDealDamage = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;
			const float vanillaDelta = before - afterDealDamage;
			if (avOwner && vanillaDelta > 0.0f) {
				avOwner->RestoreActorValue(RE::ActorValue::kHealth, vanillaDelta);
			}
		}

		// Entrada real del arma en el inventario del atacante (con forja y encantamiento), o nullptr.
		RE::InventoryEntryData* FindInventoryEntry(RE::Actor* a_owner, RE::TESBoundObject* a_object)
		{
			auto* changes = a_owner ? a_owner->GetInventoryChanges() : nullptr;
			if (!changes || !changes->entryList) {
				return nullptr;
			}

			for (auto* entry : *changes->entryList) {
				if (entry && entry->object == a_object) {
					return entry;
				}
			}
			return nullptr;
		}

		// Golpe con el daño real del arma × a_mult por el pipeline nativo; el stagger del motor se anula.
		void ApplyWeaponHit(RE::Actor* a_attacker, RE::Actor* a_target, float a_mult, const RE::NiPoint3& a_hitPosition)
		{
			auto* manager = Weapon::WeaponManager::GetSingleton();
			auto* weapon = manager ? manager->GetActiveWeapon() : nullptr;
			if (!weapon) {
				logs::warn("Combat::ApplyWeaponHit: no hay arma activa en el ciclo, golpe sin daño.");
				return;
			}

			std::optional<RE::InventoryEntryData> tempEntry;
			auto*                                 entry = FindInventoryEntry(a_attacker, weapon);
			if (!entry) {
				tempEntry.emplace(weapon, 1);
				entry = std::addressof(*tempEntry);
			}

			auto* hitData = RE::HitData::Create(a_attacker, a_target, entry, false);
			if (!hitData) {
				logs::warn("Combat::ApplyWeaponHit: HitData::Create devolvió nullptr.");
				return;
			}

			const float fullDamage = hitData->totalDamage;
			hitData->totalDamage = fullDamage * a_mult;
			hitData->stagger = 0.0f;
			hitData->hitPosition = a_hitPosition;
			auto direction = a_target->GetPosition() - a_attacker->GetPosition();
			if (direction.Length() > 0.0f) {
				direction.Unitize();
			}
			hitData->hitDirection = direction;

			if (g_processHit) {
				g_processHit(a_target, *hitData);
			} else {
				ApplyDamage(a_target, hitData->totalDamage);
				NotifyHit(a_attacker, a_target, hitData->totalDamage);
			}

			hitData->~HitData();
			RE::free(hitData);
		}

		// Hazard del impacto actual y generación para descartar una colocación diferida obsoleta.
		RE::ObjectRefHandle g_activeHazard;
		std::uint32_t       g_hazardGeneration = 0;

		// Capa de colisión de la réplica antes de clavarse (kUnidentified = nada que restaurar).
		RE::COL_LAYER g_embeddedReplicaLayer = RE::COL_LAYER::kUnidentified;

		// Coloca a_form sobre a_anchor como hazard activo, con el atacante como dueño.
		RE::TESObjectREFR* PlaceHazard(RE::BGSHazard* a_form, RE::Actor* a_attacker, RE::TESObjectREFR& a_anchor, std::uint32_t a_generation)
		{
			if (!a_form) {
				return nullptr;
			}
			if (a_generation != g_hazardGeneration) {
				return nullptr;
			}

			auto ref = a_anchor.PlaceObjectAtMe(a_form, false);
			if (!ref) {
				logs::warn("Combat::PlaceHazard: PlaceObjectAtMe devolvió nullptr.");
				return nullptr;
			}

			if (auto* hazard = ref->As<RE::Hazard>()) {
				hazard->GetHazardRuntimeData().ownerActor = RE::ActorHandle(a_attacker);
			} else {
				logs::warn("Combat::PlaceHazard: la referencia colocada no es un RE::Hazard, sin dueño asignado.");
			}

			if (auto previous = g_activeHazard.get()) {
				previous->Disable();
				previous->SetDelete(true);
			}
			g_activeHazard = ref->CreateRefHandle();
			return ref.get();
		}
	}

	void Init()
	{
		g_processHit = GameOffsets::ResolveProcessHit();
		if (g_processHit) {
			logs::info("Combat::Init: función nativa de procesar golpe resuelta, se usa el pipeline de combate real.");
		} else {
			logs::warn("Combat::Init: no se pudo resolver la función nativa de procesar golpe (VR, o sitio del call inesperado), se usa el camino de respaldo.");
		}
	}

	void BeginEmbeddedEffect(
		RE::Actor*                                                a_attacker,
		RE::Actor*                                                a_target,
		RE::ObjectRefHandle                                       a_replicaHandle,
		std::function<void(RE::ActorHandle, const RE::NiPoint3&)> a_onStuck,
		std::function<void()>                                     a_onAutoRecall,
		std::function<void(Physics::TickToken)>                   a_onTickStarted)
	{
		if (!a_attacker || !a_target) {
			return;
		}

		// Punto de golpe: la réplica, o el objetivo como respaldo.
		auto               impactReplica = a_replicaHandle.get();
		const RE::NiPoint3 hitPosition = impactReplica ? impactReplica->GetPosition() : a_target->GetPosition();
		ApplyWeaponHit(a_attacker, a_target, Settings::GetThrowHitMult(), hitPosition);

		// La inmunidad la decide la condición del efecto en la Creation Kit.
		a_onStuck(RE::ActorHandle(a_target), RE::NiPoint3{});

		if (auto* spell = Forms::paralysisSpell) {
			a_target->AddSpell(spell);
		}

		// Efecto de parálisis, para comprobar si quedó activo (AddSpell siempre tiene éxito).
		auto* paralysisEffect = Forms::paralysisEffect;

		// Desplazamiento en el espacio local del hueso más cercano; cada tick se reaplica con su transformación.
		// Sin hueso, el nodo raíz.
		auto                    replica = a_replicaHandle.get();
		const RE::BSFixedString boneName = replica ? ActorUtils::FindNearestBoneName(a_target, replica->GetPosition()) : RE::BSFixedString{};
		auto*                   rootNode = a_target->Get3D();
		auto*                   trackedNode = (!boneName.empty() && rootNode) ? rootNode->GetObjectByName(boneName) : rootNode;
		RE::NiPoint3            localOffset{};
		if (replica && trackedNode) {
			localOffset = trackedNode->world.rotate.Transpose() * (replica->GetPosition() - trackedNode->world.translate);
		}
		RE::ActorHandle targetHandle(a_target);

		// Sin colisión mientras está clavada, para no empujar el ragdoll.
		// La restaura RestoreReplicaCollision al desclavar.
		if (auto* replica3D = replica ? replica->Get3D() : nullptr) {
			g_embeddedReplicaLayer = replica3D->GetCollisionLayer();
			replica3D->SetCollisionLayer(RE::COL_LAYER::kNonCollidable);
		}

		// El arma se queda en el ángulo que tenía al impactar.
		auto token = Physics::StartTickLoop(a_replicaHandle, [targetHandle, localOffset, boneName, paralysisEffect, onAutoRecall = a_onAutoRecall, totalElapsed = 0.0f, effectConfirmed = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			auto target = targetHandle.get();
			if (!target) {
				// El actor ya no existe: la réplica se queda donde está.
				return false;
			}

			auto* currentRoot = target->Get3D();
			auto* currentNode = (!boneName.empty() && currentRoot) ? currentRoot->GetObjectByName(boneName) : currentRoot;
			if (!currentNode) {
				currentNode = currentRoot;
			}
			const auto nextPos = currentNode ?
			                         currentNode->world.translate + currentNode->world.rotate * localOffset :
			                         target->GetPosition();
			a_refr.SetPosition(nextPos);
			Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());

			totalElapsed += a_deltaSeconds;

			// Comprueba cada tick con AsMagicTarget() si la parálisis quedó activa.
			if (!effectConfirmed) {
				auto* magicTarget = target->AsMagicTarget();
				if (paralysisEffect && magicTarget && magicTarget->HasMagicEffect(paralysisEffect)) {
					effectConfirmed = true;
				} else if (totalElapsed >= Constants::kImmunityCheckDelay) {
					logs::info("Combat: el objetivo no se paralizó (inmune, o el efecto no se encontró), recuperando automáticamente.");
					onAutoRecall();
					return false;
				}
			}

			// Pasado Constants::kEmbeddedMaxDuration el arma vuelve sola.
			if (totalElapsed >= Constants::kEmbeddedMaxDuration) {
				logs::info("Combat: duración máxima clavada alcanzada, recuperando automáticamente.");
				onAutoRecall();
				return false;
			}

			return true;
		});

		a_onTickStarted(token);
	}

	std::uint32_t GetHazardGeneration()
	{
		return g_hazardGeneration;
	}

	void SpawnActorHazard(RE::Actor* a_attacker, RE::Actor& a_target, std::uint32_t a_generation)
	{
		// Desactivable con [Damage] HazardOnActor.
		if (!Settings::GetHazardOnActor()) {
			return;
		}

		if (auto* ref = PlaceHazard(Forms::actorHazard, a_attacker, a_target, a_generation)) {
			const auto pos = ref->GetPosition();
		}
	}

	void SpawnSurfaceHazard(RE::Actor* a_attacker, RE::TESObjectREFR& a_anchor, const RE::NiPoint3& a_point, const RE::NiPoint3& a_normal, std::uint32_t a_generation)
	{
		// Desactivable con [Damage] HazardOnSurface.
		if (!Settings::GetHazardOnSurface()) {
			return;
		}

		auto* ref = PlaceHazard(Forms::surfaceHazard, a_attacker, a_anchor, a_generation);
		if (!ref) {
			return;
		}

		// Orienta el eje Z del hazard según la normal: x = acos(n.z), z = atan2(n.x, n.y).
		RE::NiPoint3 n = a_normal;
		if (n.Length() < 0.001f) {
			n = { 0.0f, 0.0f, 1.0f };
		} else {
			n.Unitize();
		}
		const float        nz = n.z > 1.0f ? 1.0f : (n.z < -1.0f ? -1.0f : n.z);
		const RE::NiPoint3 angle{ std::acos(nz), 0.0f, std::atan2(n.x, n.y) };

		ref->SetPosition(a_point);
		ref->SetAngle(angle);
	}

	void RemoveImpactHazard()
	{
		++g_hazardGeneration;
		if (auto hazard = g_activeHazard.get()) {
			hazard->Disable();
			hazard->SetDelete(true);
		}
		g_activeHazard = {};
	}

	void RestoreReplicaCollision(RE::TESObjectREFR* a_replica)
	{
		const auto layer = g_embeddedReplicaLayer;
		g_embeddedReplicaLayer = RE::COL_LAYER::kUnidentified;
		if (layer == RE::COL_LAYER::kUnidentified) {
			return;
		}

		if (auto* replica3D = a_replica ? a_replica->Get3D() : nullptr) {
			replica3D->SetCollisionLayer(layer);
		}
	}

	void EndEmbeddedEffect(RE::Actor* a_target)
	{
		if (!a_target) {
			return;
		}

		if (auto* spell = Forms::paralysisSpell) {
			a_target->RemoveSpell(spell);
		}
	}

	void ApplyReturnHit(RE::Actor* a_attacker, RE::Actor* a_target, const RE::NiPoint3& a_hitPosition)
	{
		if (!a_attacker || !a_target) {
			return;
		}

		// El golpe se aplica siempre, aunque el multiplicador sea 0.
		ApplyWeaponHit(a_attacker, a_target, Settings::GetReturnHitMult(), a_hitPosition);

		// Desactivable con [Damage] ReturnStagger.
		if (!Settings::GetReturnStagger()) {
			return;
		}

		// Tambaleo propio con las graph variables staggerMagnitude/staggerDirection y el evento staggerStart.
		a_target->SetGraphVariableFloat("staggerMagnitude", Constants::kStaggerMagnitude);
		a_target->SetGraphVariableFloat("staggerDirection", 0.0f);  // de frente
		a_target->NotifyAnimationGraph("staggerStart");
	}
}
