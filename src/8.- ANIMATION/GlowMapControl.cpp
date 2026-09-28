// Glow de la textura del martillo -- ver GlowMapControl.h.

#include "8.- ANIMATION/GlowMapControl.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "1.- CORE/Settings.h"
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
		// Todo en el hilo principal (callback del bucle de tick y el
		// arranque diferido de EnsureRunning), sin mutex.
		Physics::TickToken g_tickToken;

		// emissiveMult original de cada malla con glow map, por nombre de
		// malla. Se guarda antes de la primera escritura: todas las copias
		// del arma salen del mismo .nif, así que el valor es el mismo para
		// cualquier instancia (equipada, réplica, recargada tras cambiar
		// de celda).
		std::unordered_map<std::string, float> g_originalMults;

		// Fundido 0..1 hacia el objetivo (condición cumplida y modo no
		// apagado), fase del pulso y temporizador de la búsqueda de
		// criaturas.
		float g_fade = -1.0f;  // < 0: todavía sin inicializar (sin fundido al arrancar)
		float g_pulsePhase = 0.0f;
		float g_scanTimer = 0.0f;
		bool  g_creatureNearby = false;

		// Actores cargados en proceso alto (los que están cerca y
		// actualizándose de verdad) vivos, dentro del radio y cuya raza
		// lleve una de las keywords marcadas.
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

				if ((dragons && race->HasKeywordString(Constants::kGlowMapDragonKeyword)) ||
					(undead && race->HasKeywordString(Constants::kGlowMapUndeadKeyword)) ||
					(daedra && race->HasKeywordString(Constants::kGlowMapDaedraKeyword))) {
					if (actor->GetPosition().GetDistance(playerPos) <= radius) {
						return true;
					}
				}
			}
			return false;
		}

		// Diagnóstico de una sola vez: si bajo el arma equipada no aparece
		// ninguna malla con glow map, vuelca el tipo de material de cada
		// malla que sí hay.
		bool g_missingReported = false;

		void DumpMaterials(RE::NiAVObject* a_object)
		{
			if (!a_object) {
				return;
			}

			if (auto* geometry = a_object->AsGeometry()) {
				auto* shader = geometry->GetGeometryRuntimeData().shaderProperty.get();
				auto* lighting = shader ? netimmerse_cast<RE::BSLightingShaderProperty*>(shader) : nullptr;
				auto* material = lighting ? lighting->GetBaseMaterial() : nullptr;
				logs::info("GlowMapControl:   malla '{}' -- BSLightingShaderProperty {} -- feature {}.",
					geometry->name.c_str(), lighting != nullptr, material ? static_cast<int>(material->GetFeature()) : -1);
				return;
			}

			if (auto* node = a_object->AsNode()) {
				for (auto& child : node->GetChildren()) {
					DumpMaterials(child.get());
				}
			}
		}

		// Escribe a_factor × original en cada malla con glow map bajo
		// a_object (recorrido recursivo del árbol de nodos). Devuelve
		// cuántas ha encontrado.
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
					const auto [it, inserted] = g_originalMults.try_emplace(name, lighting->emissiveMult);
					if (inserted) {
						// Diagnóstico: confirma en el log que se encuentra la
						// malla y con qué valor original.
						logs::info("GlowMapControl: malla con glow map '{}', emissiveMult original {:.3f}.", name, it->second);
					}
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

		// 3D del arma equipada según los datos de biped del actor
		// (BIPOBJECT::partClone de la pieza cuyo item es a_weapon): el
		// modelo real, esté colgado de la mano o del nodo de envainado.
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

		// Arma equipada en tercera y primera persona, desenvainada o
		// envainada. Historial (probado en el juego, 2026-09-28): buscar
		// solo bajo el hueso "WEAPON" dejaba el pulso congelado al
		// envainar (el arma pasa al nodo de la cadera, "WeaponAxe"), y
		// buscar el nodo de giro por nombre ("Mjolnir") desde la raíz del
		// actor no lo encontraba en el arma equipada (sí en la réplica).
		// Si el biped no da el modelo, se cae al hueso "WEAPON", que sí
		// funcionaba desenvainada.
		void ApplyToEquipped(RE::Actor& a_player, float a_factor)
		{
			auto* rightHand = a_player.GetEquippedObject(false);
			auto* weapon = rightHand ? rightHand->As<RE::TESObjectWEAP>() : nullptr;
			if (!weapon || !weapon->HasKeywordString(Constants::kThrowableWeaponKeyword)) {
				return;
			}

			for (const bool firstPerson : { false, true }) {
				auto*     model = FindEquippedModel(a_player, weapon, firstPerson);
				const int source = model ? 1 : 2;
				if (!model) {
					auto* root = a_player.Get3D(firstPerson);
					model = root ? root->GetObjectByName("WEAPON") : nullptr;
				}

				// Diagnóstico: vía por la que se localiza el modelo en
				// tercera persona (0 = ninguna), solo cuando cambia.
				static int lastSource = -1;
				const int  currentSource = model ? source : 0;
				if (!firstPerson && currentSource != lastSource) {
					lastSource = currentSource;
					logs::info("GlowMapControl: modelo equipado (tercera persona) vía {}.",
						currentSource == 1 ? "biped" : (currentSource == 2 ? "hueso WEAPON" : "ninguna"));
				}

				if (ApplyToTree(model, a_factor) == 0 && model && !firstPerson && !g_missingReported) {
					g_missingReported = true;
					logs::warn("GlowMapControl: ninguna malla con glow map en el modelo equipado '{}' (tercera persona). Mallas encontradas:", model->name.c_str());
					DumpMaterials(model);
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
					const bool nearby = ScanForCreatures(*player);
					if (nearby != g_creatureNearby) {
						logs::info("GlowMapControl: criatura cerca -> {}.", nearby);
					}
					g_creatureNearby = nearby;
				}
			} else {
				g_scanTimer = 0.0f;
			}

			const bool active = mode != Settings::GlowMode::kOff &&
			                    (condition == Settings::GlowCondition::kAlways || g_creatureNearby);
			const float target = active ? 1.0f : 0.0f;

			// Primer tick: directamente al objetivo, sin fundido (al
			// cargar partida el arma no debe verse encenderse).
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
		// Se llama desde sinks cuyo hilo no está garantizado (p. ej. el
		// cierre de la pantalla de carga) y desde el evento de equipar, que
		// puede dispararse dentro de una tarea ya en ejecución. El trabajo
		// real se hace siempre en el hilo principal vía Scheduler (hilo
		// aparte que reencola con AddTask, seguro en los dos casos): así
		// dos peticiones simultáneas llegan en fila, la segunda ve el
		// bucle ya en marcha y no se arrancan dos.
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
			logs::info("GlowMapControl: bucle de glow del arma en marcha.");
		});
	}
}
