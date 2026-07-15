// Implementación del sistema de daño.
// Calcula y aplica los efectos de impacto sobre actores.

#include "7.- COMBAT/DamageManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/GameOffsets.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <SimpleIni.h>

#include <thread>

namespace Combat
{
	namespace
	{
		enum class DamageMode
		{
			kDirect,  // Valores fijos, configurados directamente.
			kLevel    // Escala con el nivel del atacante (siempre el jugador en este plugin).
		};

		struct DamageConfig
		{
			DamageMode mode{ DamageMode::kDirect };

			// Modo Direct: daño fijo del golpe inicial y del goteo
			// continuo mientras sigue clavada.
			float directHit{ 25.0f };
			float directDot{ 6.0f };

			// Modo Level: daño = base + porNivel * nivel del atacante —
			// a niveles bajos hace poco daño, más a medida que sube.
			float levelHitBase{ 5.0f };
			float levelHitPerLevel{ 1.5f };
			float levelDotBase{ 1.0f };
			float levelDotPerLevel{ 0.3f };

			// Punto 9: fracción del daño de golpe normal aplicada en un
			// golpe durante el regreso (0 = sin daño, solo stagger).
			float returnHitMultiplier{ 0.5f };
		};

		DamageConfig g_config{};

		float ComputeHitDamage(RE::Actor* a_attacker)
		{
			if (g_config.mode == DamageMode::kLevel) {
				const float level = a_attacker ? static_cast<float>(a_attacker->GetLevel()) : 1.0f;
				return g_config.levelHitBase + g_config.levelHitPerLevel * level;
			}

			return g_config.directHit;
		}

		float ComputeDotDamage(RE::Actor* a_attacker)
		{
			if (g_config.mode == DamageMode::kLevel) {
				const float level = a_attacker ? static_cast<float>(a_attacker->GetLevel()) : 1.0f;
				return g_config.levelDotBase + g_config.levelDotPerLevel * level;
			}

			return g_config.directDot;
		}

		void ApplyDamage(RE::Actor* a_target, float a_amount)
		{
			// Actor hereda ActorValueOwner (de donde viene
			// DamageActorValue) como una base más de una herencia
			// múltiple, no la primera — su offset dentro de Actor cambia
			// entre versiones del juego (0xB0 antes de 1.6.629, 0xB8 en
			// AE/posteriores, ver Actor.h). Acceder a DamageActorValue
			// directamente sobre un Actor* usa el offset fijo que decida
			// el compilador para este build multi-runtime, que no tiene
			// por qué coincidir con la versión real del juego — crasheaba
			// el juego (comprobado). Actor::AsActorValueOwner() es el
			// accessor de commonlibsse-ng que calcula el offset correcto
			// según la versión real detectada en tiempo de ejecución.
			auto*       avOwner = a_target->AsActorValueOwner();
			const float before = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;

			if (avOwner) {
				avOwner->DamageActorValue(RE::ActorValue::kHealth, a_amount);
			}

			// Diagnóstico temporal (ver CHANGELOG): confirmar en el juego
			// si Actor::HandleHealthDamage (llamado aparte en NotifyHit,
			// justo después de esto en cada punto de llamada) vuelve a
			// restar vida por su cuenta o no — sin fuente que lo
			// documente con certeza. Comparar el "después" de este log
			// con el "antes"/"después" del de NotifyHit.
			const float after = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;
			logs::info(
				"Combat::ApplyDamage: \"{}\" vida {:.1f} -> {:.1f} (DamageActorValue, importe pedido {:.1f})",
				a_target->GetName(), before, after, a_amount);
		}

		// Actor::HandleHealthDamage + Actor::SetBeenAttacked (probado en
		// el juego, ver CHANGELOG) no bastan por sí solos para que la IA
		// reaccione (perseguir, aggro) ni para que un aliado lo tome como
		// agresión. GameOffsets::DealDamage sí (ver esa declaración para
		// de dónde sale el ID y por qué solo es segura en SE/AE) — pero
		// calcula su propio daño a partir del arma que el atacante tenga
		// equipada en ese instante, y durante todo nuestro ciclo la mano
		// va vacía (arma real oculta, punto 2), así que ese importe no es
		// el que queremos (con toda probabilidad, daño de puñetazo). Se
		// usa aquí solo por su efecto colateral real (el aviso a IA de
		// combate/crimen), revirtiendo lo que le haya hecho a la vida y
		// aplicando en su lugar el importe ya calculado por nuestra
		// configuración de daño (INI, ver ApplyDamage).
		void NotifyHit(RE::Actor* a_attacker, RE::Actor* a_target, float a_amount)
		{
			auto* avOwner = a_target->AsActorValueOwner();

			if (REL::Module::IsVR()) {
				a_target->SetBeenAttacked(true);
				a_target->HandleHealthDamage(a_attacker, a_amount);
				logs::info("Combat::NotifyHit (VR, sin DealDamage verificado): \"{}\" avisado.", a_target->GetName());
				return;
			}

			const float before = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;

			GameOffsets::DealDamage(a_attacker, a_target, nullptr, false);

			const float afterDealDamage = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;
			const float vanillaDelta = before - afterDealDamage;
			if (avOwner && vanillaDelta > 0.0f) {
				avOwner->RestoreActorValue(RE::ActorValue::kHealth, vanillaDelta);
			}

			// Diagnóstico (pendiente de confirmar en el juego): si "tras
			// compensar" no coincide con "antes", el arma equipada en ese
			// instante sí influye de otra forma no prevista, o
			// RestoreActorValue no deshace exactamente lo que hizo
			// DealDamage.
			const float afterCompensation = avOwner ? avOwner->GetActorValue(RE::ActorValue::kHealth) : 0.0f;
			logs::info(
				"Combat::NotifyHit: \"{}\" vida {:.1f} -> {:.1f} (DealDamage, sin usar) -> {:.1f} (compensado, debe = {:.1f})",
				a_target->GetName(), before, afterDealDamage, afterCompensation, before);
		}
	}

	void Init()
	{
		CSimpleIniA ini;
		ini.SetUnicode();

		if (ini.LoadFile(Constants::kInputConfigPath) < 0) {
			logs::warn("Combat::Init: no se encontró {}, se usa daño directo por defecto.", Constants::kInputConfigPath);
			return;
		}

		const std::string_view mode = ini.GetValue("Damage", "Mode", "Direct");
		g_config.mode = mode == "Level" ? DamageMode::kLevel : DamageMode::kDirect;

		g_config.directHit = static_cast<float>(ini.GetDoubleValue("Damage", "HitDamage", g_config.directHit));
		g_config.directDot = static_cast<float>(ini.GetDoubleValue("Damage", "DotDamage", g_config.directDot));
		g_config.levelHitBase = static_cast<float>(ini.GetDoubleValue("Damage", "LevelHitBase", g_config.levelHitBase));
		g_config.levelHitPerLevel = static_cast<float>(ini.GetDoubleValue("Damage", "LevelHitPerLevel", g_config.levelHitPerLevel));
		g_config.levelDotBase = static_cast<float>(ini.GetDoubleValue("Damage", "LevelDotBase", g_config.levelDotBase));
		g_config.levelDotPerLevel = static_cast<float>(ini.GetDoubleValue("Damage", "LevelDotPerLevel", g_config.levelDotPerLevel));
		g_config.returnHitMultiplier = static_cast<float>(ini.GetDoubleValue("Damage", "ReturnHitMultiplier", g_config.returnHitMultiplier));

		logs::info(
			"Combat::Init: modo de daño = {} (golpe: {:.1f}, dot: {:.1f} / nivel: base {:.1f}+{:.1f}, dot base {:.1f}+{:.1f}), multiplicador golpe en regreso = {:.2f}",
			g_config.mode == DamageMode::kLevel ? "Level" : "Direct",
			g_config.directHit, g_config.directDot,
			g_config.levelHitBase, g_config.levelHitPerLevel,
			g_config.levelDotBase, g_config.levelDotPerLevel,
			g_config.returnHitMultiplier);
	}

	void BeginEmbeddedEffect(
		RE::Actor*                              a_attacker,
		RE::Actor*                              a_target,
		RE::ObjectRefHandle                     a_replicaHandle,
		std::function<void(RE::ActorHandle)>    a_onStuck,
		std::function<void()>                   a_onAutoRecall,
		std::function<void(Physics::TickToken)> a_onTickStarted)
	{
		if (!a_attacker || !a_target) {
			return;
		}

		const float hitDamage = ComputeHitDamage(a_attacker);
		ApplyDamage(a_target, hitDamage);
		NotifyHit(a_attacker, a_target, hitDamage);

		// Quién es inmune (dragones, criaturas concretas...) lo decide
		// solo la condición del propio efecto en la Creation Kit — no se
		// duplica esa lógica aquí. El sondeo de más abajo
		// (MagicTarget::HasMagicEffect) ya cubre cualquier caso de
		// inmunidad de forma genérica, así que cambiar quién es inmune
		// solo requiere tocar la condición en el CK, nunca este código.
		a_onStuck(RE::ActorHandle(a_target));

		if (auto* spell = RE::TESForm::LookupByEditorID<RE::SpellItem>(Constants::kEmbeddedParalysisSpell)) {
			a_target->AddSpell(spell);
		} else {
			logs::warn(
				"Combat::BeginEmbeddedEffect: no se encontró el hechizo \"{}\" (revisa que exista en la Creation Kit).",
				Constants::kEmbeddedParalysisSpell);
		}

		// El propio efecto mágico (EffectSetting) dentro del hechizo,
		// distinto del hechizo en sí — hace falta para comprobar más
		// abajo, con MagicTarget::HasMagicEffect, si de verdad ha quedado
		// activo en el objetivo (AddSpell siempre tiene éxito aunque la
		// condición del efecto se lo impida, ver Constants::kEmbeddedParalysisEffect).
		auto* paralysisEffect = RE::TESForm::LookupByEditorID<RE::EffectSetting>(Constants::kEmbeddedParalysisEffect);
		if (!paralysisEffect) {
			logs::warn(
				"Combat::BeginEmbeddedEffect: no se encontró el efecto \"{}\" (revisa que exista en la Creation Kit).",
				Constants::kEmbeddedParalysisEffect);
		}

		// Desplazamiento respecto al hueso más cercano al punto de impacto
		// (ActorUtils::FindNearestBoneName) en el instante del impacto,
		// guardado en su espacio LOCAL (girado con su rotación de ese
		// momento) en vez de un vector fijo en espacio del mundo — un
		// vector fijo se notaba flotando lejos del cuerpo en cuanto el
		// actor caía por la parálisis (comprobado en el juego): la
		// orientación cambia por completo al caer, y un desplazamiento en
		// espacio del mundo no la sigue. Cada tick se reconvierte a
		// espacio del mundo con la rotación actual de ESE hueso (ver más
		// abajo), así que el arma sigue el movimiento real del cuerpo
		// (respiración, tembleque de la parálisis, reacciones de golpe) en
		// vez de solo el del nodo raíz, que antes se notaba flotando —
		// comprobado en el juego. Se guarda el *nombre* del hueso, no un
		// puntero crudo, y se resuelve de nuevo cada tick (ver más abajo),
		// mismo motivo de cautela que ya había para el nodo raíz: el 3D
		// podría recargarse. Si no se encuentra ningún hueso (actor sin
		// 3D), se cae al nodo raíz — mismo comportamiento que antes.
		auto                    replica = a_replicaHandle.get();
		const RE::BSFixedString boneName = replica ? ActorUtils::FindNearestBoneName(a_target, replica->GetPosition()) : RE::BSFixedString{};
		auto*                   rootNode = a_target->Get3D();
		auto*                   trackedNode = (!boneName.empty() && rootNode) ? rootNode->GetObjectByName(boneName) : rootNode;
		RE::NiPoint3            localOffset{};
		if (replica && trackedNode) {
			localOffset = trackedNode->world.rotate.Transpose() * (replica->GetPosition() - trackedNode->world.translate);
		}
		RE::ActorHandle targetHandle(a_target);

		auto token = Physics::StartTickLoop(a_replicaHandle, [a_attacker, targetHandle, localOffset, boneName, paralysisEffect, onAutoRecall = a_onAutoRecall, totalElapsed = 0.0f, dotElapsed = 0.0f, effectConfirmed = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			auto target = targetHandle.get();
			if (!target) {
				// El actor ya no existe (p. ej. la celda se ha
				// descargado); la réplica se queda donde estaba, sigue
				// pudiendo recuperarse con el botón.
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

			// Comprobación exacta (no una inferencia): MagicTarget no es
			// la primera clase base de Actor tampoco, así que se usa
			// Actor::AsMagicTarget() (accessor versionado, mismo motivo
			// que ActorValueOwner/ActorState) para preguntar directamente
			// si nuestro efecto concreto está activo de verdad. AddSpell
			// siempre tiene éxito aunque la condición del propio efecto
			// (inmune a parálisis, dragón...) le impida aplicarse — el
			// motor necesita al menos un tick para reflejarlo, así que se
			// comprueba cada tick hasta confirmarse o agotar el margen.
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

			// Nerfeo pedido tras las primeras pruebas: duración máxima
			// clavada, pasado ese tiempo el arma vuelve sola.
			if (totalElapsed >= Constants::kEmbeddedMaxDuration) {
				logs::info("Combat: duración máxima clavada alcanzada, recuperando automáticamente.");
				onAutoRecall();
				return false;
			}

			dotElapsed += a_deltaSeconds;
			if (dotElapsed >= Constants::kEmbeddedDamageInterval) {
				dotElapsed = 0.0f;
				const float dotDamage = ComputeDotDamage(a_attacker);
				ApplyDamage(target.get(), dotDamage);
				NotifyHit(a_attacker, target.get(), dotDamage);
			}

			return true;
		});

		a_onTickStarted(token);
	}

	void EndEmbeddedEffect(RE::Actor* a_target)
	{
		if (!a_target) {
			return;
		}

		if (auto* spell = RE::TESForm::LookupByEditorID<RE::SpellItem>(Constants::kEmbeddedParalysisSpell)) {
			a_target->RemoveSpell(spell);
		}
	}

	void ApplyReturnHit(RE::Actor* a_attacker, RE::Actor* a_target)
	{
		if (!a_attacker || !a_target) {
			return;
		}

		logs::info("Combat::ApplyReturnHit: golpe durante el regreso contra \"{}\".", a_target->GetName());

		// El aviso de golpe (NotifyHit) se dispara siempre, aunque el
		// daño esté desactivado por INI (returnHitMultiplier == 0): la
		// reacción del objetivo (perseguir, o que un aliado se lo tome
		// como agresión) no debe depender de esa opción, solo de que
		// hubo un golpe de verdad.
		const float amount = g_config.returnHitMultiplier > 0.0f ? ComputeHitDamage(a_attacker) * g_config.returnHitMultiplier : 0.0f;
		if (amount > 0.0f) {
			ApplyDamage(a_target, amount);
		}
		NotifyHit(a_attacker, a_target, amount);

		auto* spell = RE::TESForm::LookupByEditorID<RE::SpellItem>(Constants::kStaggerSpell);
		if (!spell) {
			logs::warn(
				"Combat::ApplyReturnHit: no se encontró el hechizo \"{}\" (revisa que exista en la Creation Kit).",
				Constants::kStaggerSpell);
			return;
		}

		a_target->AddSpell(spell);

		// A diferencia de la parálisis (que dura mientras el arma siga
		// clavada, y se retira al recuperarla), el empujón de Stagger ya
		// ha ocurrido en el instante de AddSpell (ActiveEffect::Start) —
		// solo queda retirar la habilidad para no dejarla colgada en la
		// lista de hechizos del actor. Un hilo que duerme y reencola en
		// el hilo principal (mismo patrón que el resto del proyecto, ver
		// CLAUDE.md), no un bucle de tick: no hace falta repetir nada
		// mientras tanto.
		RE::ActorHandle targetHandle(a_target);
		std::thread([targetHandle, spell]() {
			std::this_thread::sleep_for(Constants::kStaggerSpellDuration);
			SKSE::GetTaskInterface()->AddTask([targetHandle, spell]() {
				if (auto target = targetHandle.get()) {
					target->RemoveSpell(spell);
				}
			});
		}).detach();
	}
}
