// Implementación del sistema de daño.
// Calcula y aplica los efectos de impacto sobre actores.

#include "7.- COMBAT/DamageManager.h"

#include "1.- CORE/Constants.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <SimpleIni.h>

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
			if (auto* avOwner = a_target->AsActorValueOwner()) {
				avOwner->DamageActorValue(RE::ActorValue::kHealth, a_amount);
			}
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

		logs::info(
			"Combat::Init: modo de daño = {} (golpe: {:.1f}, dot: {:.1f} / nivel: base {:.1f}+{:.1f}, dot base {:.1f}+{:.1f})",
			g_config.mode == DamageMode::kLevel ? "Level" : "Direct",
			g_config.directHit, g_config.directDot,
			g_config.levelHitBase, g_config.levelHitPerLevel,
			g_config.levelDotBase, g_config.levelDotPerLevel);
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

		ApplyDamage(a_target, ComputeHitDamage(a_attacker));

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

		// Desplazamiento respecto al actor en el instante del impacto,
		// guardado en su espacio LOCAL (girado con su rotación de ese
		// momento) en vez de un vector fijo en espacio del mundo — un
		// vector fijo se notaba flotando lejos del cuerpo en cuanto el
		// actor caía por la parálisis (comprobado en el juego): la
		// orientación cambia por completo al caer, y un desplazamiento en
		// espacio del mundo no la sigue. Cada tick se reconvierte a
		// espacio del mundo con la rotación actual del actor (ver más
		// abajo), así que el arma gira con él en vez de quedarse fija.
		// Sigue siendo una aproximación sobre el nodo raíz del actor, no
		// un hueso concreto — no hay API para localizar el hueso más
		// cercano a un punto, ver CLAUDE.md.
		auto         replica = a_replicaHandle.get();
		auto*        targetNode = a_target->Get3D();
		RE::NiPoint3 localOffset{};
		if (replica && targetNode) {
			localOffset = targetNode->world.rotate.Transpose() * (replica->GetPosition() - targetNode->world.translate);
		}
		RE::ActorHandle targetHandle(a_target);

		auto token = Physics::StartTickLoop(a_replicaHandle, [a_attacker, targetHandle, localOffset, paralysisEffect, onAutoRecall = a_onAutoRecall, totalElapsed = 0.0f, dotElapsed = 0.0f, effectConfirmed = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			auto target = targetHandle.get();
			if (!target) {
				// El actor ya no existe (p. ej. la celda se ha
				// descargado); la réplica se queda donde estaba, sigue
				// pudiendo recuperarse con el botón.
				return false;
			}

			auto*      currentNode = target->Get3D();
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
				ApplyDamage(target.get(), ComputeDotDamage(a_attacker));
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
}
