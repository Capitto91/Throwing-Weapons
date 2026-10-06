// Glow de la textura del martillo -- ver GlowMapControl.h.

#include "8.- ANIMATION/GlowMapControl.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Forms.h"
#include "1.- CORE/Scheduler.h"
#include "1.- CORE/Settings.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "3.- WEAPON/WeaponManager.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <cmath>
#include <numbers>
#include <string>
#include <unordered_map>

namespace Animation::GlowMapControl
{
	namespace
	{
		// Estado del bucle, solo hilo principal.
		Physics::TickToken g_tickToken;

		// emissiveMult original de cada malla con glow map, por nombre (100% del brillo).
		std::unordered_map<std::string, float> g_originalMults;

		// Fundido 0..1, fase del pulso y temporizador de la búsqueda de criaturas.
		float g_fade = -1.0f;  // < 0: primer tick, sin fundido
		float g_pulsePhase = 0.0f;
		float g_scanTimer = 0.0f;
		bool  g_creatureNearby = false;

		// ¿Hay un actor vivo cercano (proceso alto) dentro del radio con una keyword de raza marcada?
		bool ScanForCreatures(RE::Actor& a_player)
		{
			const bool dragons = Settings::GetGlowNearDragons();
			const bool undead = Settings::GetGlowNearUndead();
			const bool daedra = Settings::GetGlowNearDaedra();
			if (!dragons && !undead && !daedra) {
				return false;
			}

			auto* processLists = RE::ProcessLists::GetSingleton();
			if (!processLists) {
				return false;
			}

			const float radius = Settings::GetGlowRadius();
			const auto  playerPos = a_player.GetPosition();

			for (auto& handle : processLists->highActorHandles) {
				auto  actorPtr = handle.get();
				auto* actor = actorPtr.get();
				if (!actor || actor == &a_player || actor->IsDead()) {
					continue;
				}

				auto* race = actor->GetRace();
				if (!race) {
					continue;
				}

				// Una keyword que no se encontró al cargar (nullptr) no cuenta.
				const auto hasType = [race](const RE::BGSKeyword* a_keyword) { return a_keyword && race->HasKeyword(a_keyword); };
				if ((dragons && hasType(Forms::actorTypeDragon)) ||
					(undead && hasType(Forms::actorTypeUndead)) ||
					(daedra && hasType(Forms::actorTypeDaedra))) {
					if (actor->GetPosition().GetDistance(playerPos) <= radius) {
						return true;
					}
				}
			}
			return false;
		}

		// Aviso único si el modelo equipado no tiene ninguna malla con glow map.
		bool g_missingReported = false;

		// Escribe a_factor × original en cada malla con glow map bajo a_object.
		// Devuelve cuántas ha encontrado.
		int ApplyToTree(RE::NiAVObject* a_object, float a_factor)
		{
			if (!a_object) {
				return 0;
			}

			if (auto* geometry = a_object->AsGeometry()) {
				auto* shader = geometry->GetGeometryRuntimeData().shaderProperty.get();
				auto* lighting = shader ? netimmerse_cast<RE::BSLightingShaderProperty*>(shader) : nullptr;
				auto* material = lighting ? lighting->GetBaseMaterial() : nullptr;
				if (material && material->GetFeature() == RE::BSShaderMaterial::Feature::kGlowMap) {
					const std::string name = geometry->name.c_str();
					const auto        it = g_originalMults.try_emplace(name, lighting->emissiveMult).first;
					lighting->emissiveMult = it->second * a_factor;
					return 1;
				}
				return 0;
			}

			int found = 0;
			if (auto* node = a_object->AsNode()) {
				for (auto& child : node->GetChildren()) {
					found += ApplyToTree(child.get(), a_factor);
				}
			}
			return found;
		}

		// 3D del arma equipada (BIPOBJECT::partClone), en la mano o envainada.
		RE::NiAVObject* FindEquippedModel(RE::Actor& a_actor, RE::TESObjectWEAP* a_weapon, bool a_firstPerson)
		{
			const auto& biped = a_actor.GetBiped(a_firstPerson);
			if (!biped) {
				return nullptr;
			}

			for (auto& object : biped->objects) {
				if (object.item == a_weapon && object.partClone) {
					return object.partClone.get();
				}
			}
			return nullptr;
		}

		// Aplica el brillo al martillo equipado, en tercera y primera persona.
		// Busca el modelo por biped y, si no aparece, bajo el hueso "WEAPON".
		void ApplyToEquipped(RE::Actor& a_player, float a_factor)
		{
			auto* rightHand = a_player.GetEquippedObject(false);
			auto* weapon = rightHand ? rightHand->As<RE::TESObjectWEAP>() : nullptr;
			if (!ActorUtils::IsThrowableWeapon(weapon)) {
				return;
			}

			for (const bool firstPerson : { false, true }) {
				auto* model = FindEquippedModel(a_player, weapon, firstPerson);
				if (!model) {
					auto* root = a_player.Get3D(firstPerson);
					model = root ? root->GetObjectByName("WEAPON") : nullptr;
				}

				if (ApplyToTree(model, a_factor) == 0 && model && !firstPerson && !g_missingReported) {
					g_missingReported = true;
					logs::warn("GlowMapControl: ninguna malla con glow map en el modelo equipado '{}' (tercera persona).", model->name.c_str());
				}
			}
		}

		void ApplyToReplica(float a_factor)
		{
			auto replica = Weapon::WeaponManager::GetSingleton()->GetActiveReplicaHandle().get();
			if (replica) {
				ApplyToTree(replica->Get3D(), a_factor);
			}
		}

		bool Tick(RE::TESObjectREFR& a_refr, float a_deltaSeconds)
		{
			auto* player = a_refr.As<RE::Actor>();
			if (!player) {
				return true;
			}

			const auto mode = Settings::GetGlowMode();
			const auto condition = Settings::GetGlowCondition();

			if (condition == Settings::GlowCondition::kNearCreatures) {
				g_scanTimer -= a_deltaSeconds;
				if (g_scanTimer <= 0.0f) {
					g_scanTimer = Constants::kGlowMapCreatureScanIntervalSeconds;
					g_creatureNearby = ScanForCreatures(*player);
				}
			} else {
				g_scanTimer = 0.0f;
			}

			const bool active = mode != Settings::GlowMode::kOff &&
			                    (condition == Settings::GlowCondition::kAlways || g_creatureNearby);
			const float target = active ? 1.0f : 0.0f;

			// Primer tick: directo al objetivo, sin fundido.
			if (g_fade < 0.0f) {
				g_fade = target;
			} else {
				const float step = a_deltaSeconds / Constants::kGlowMapFadeSeconds;
				g_fade = g_fade < target ? (std::min)(g_fade + step, target) : (std::max)(g_fade - step, target);
			}

			float pulse = 1.0f;
			if (mode == Settings::GlowMode::kPulse) {
				g_pulsePhase = std::fmod(g_pulsePhase + a_deltaSeconds * Settings::GetGlowPulseSpeed() * 2.0f * std::numbers::pi_v<float>, 2.0f * std::numbers::pi_v<float>);
				const float wave = 0.5f + 0.5f * std::sin(g_pulsePhase);
				pulse = Constants::kGlowMapPulseMinFactor + (1.0f - Constants::kGlowMapPulseMinFactor) * wave;
			}

			const float factor = Settings::GetGlowIntensity() * pulse * g_fade;
			ApplyToEquipped(*player, factor);
			ApplyToReplica(factor);
			return true;
		}
	}

	void EnsureRunning()
	{
		// Vía Scheduler: las peticiones llegan en fila al hilo principal y solo arranca un bucle.
		(void)Scheduler::After(std::chrono::milliseconds{ 0 }, [] {
			if (g_tickToken && g_tickToken->load()) {
				return;
			}

			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				return;
			}

			g_fade = -1.0f;
			g_scanTimer = 0.0f;
			g_creatureNearby = false;
			g_tickToken = Physics::StartTickLoop(player->GetHandle(), Tick);
		});
	}
}
