// Poder Lightning Dash -- ver LightningDash.h.

#include "3.- WEAPON/LightningDash.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "2.- INPUT/InputManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "8.- ANIMATION/WeaponTrailGroup.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>

namespace Weapon::LightningDash
{
	namespace
	{
		// Estado del desplazamiento, solo hilo principal. g_generation descarta llegadas de uno cancelado.
		Physics::TickToken g_tickToken;
		bool               g_active = false;
		std::uint32_t      g_generation = 0;

		// Formularios del ESL resueltos una vez.
		template <class T>
		T* LookupForm(RE::FormID a_localFormID, std::string_view a_pluginName)
		{
			auto* dataHandler = RE::TESDataHandler::GetSingleton();
			return dataHandler ? dataHandler->LookupForm<T>(a_localFormID, a_pluginName) : nullptr;
		}

		RE::SpellItem* GetCooldownSpell()
		{
			static RE::SpellItem* spell = LookupForm<RE::SpellItem>(Constants::kLightningDashCooldownSpellLocalFormID, Constants::kSoundPluginName);
			if (!spell) {
				logs::warn("LightningDash: no se encontró el hechizo de cooldown (FormID local 0x{:03X}) en \"{}\".",
					Constants::kLightningDashCooldownSpellLocalFormID, Constants::kSoundPluginName);
			}
			return spell;
		}

		RE::EffectSetting* GetCooldownEffect()
		{
			static RE::EffectSetting* effect = LookupForm<RE::EffectSetting>(Constants::kLightningDashCooldownEffectLocalFormID, Constants::kSoundPluginName);
			if (!effect) {
				logs::warn("LightningDash: no se encontró el efecto de cooldown (FormID local 0x{:03X}) en \"{}\".",
					Constants::kLightningDashCooldownEffectLocalFormID, Constants::kSoundPluginName);
			}
			return effect;
		}

		// Busca un formulario y avisa en el log si no está. Para inicializar las cachés de abajo una sola vez.
		template <class T>
		T* LookupWithWarning(RE::FormID a_formID, std::string_view a_pluginName, std::string_view a_description)
		{
			auto* form = LookupForm<T>(a_formID, a_pluginName);
			if (!form) {
				logs::warn("LightningDash: no se encontró {} (FormID 0x{:06X}) en \"{}\".", a_description, a_formID, a_pluginName);
			}
			return form;
		}

		RE::BGSReferenceEffect* GetVisualEffect()
		{
			static RE::BGSReferenceEffect* form = LookupWithWarning<RE::BGSReferenceEffect>(Constants::kLightningDashVisualEffectLocalFormID, Constants::kSoundPluginName, "el VisualEffect");
			return form;
		}

		RE::TESImageSpaceModifier* GetImageSpaceModifier()
		{
			static RE::TESImageSpaceModifier* form = LookupWithWarning<RE::TESImageSpaceModifier>(Constants::kLightningDashImageSpaceModLocalFormID, Constants::kSoundPluginName, "el modificador de imagen");
			return form;
		}

		RE::BGSExplosion* GetDustExplosion()
		{
			static RE::BGSExplosion* form = LookupWithWarning<RE::BGSExplosion>(Constants::kLightningDashDustExplosionFormID, Constants::kLightningDashVanillaPluginName, "la explosión de polvo");
			return form;
		}

		RE::BGSExplosion* GetShockExplosion()
		{
			static RE::BGSExplosion* form = LookupWithWarning<RE::BGSExplosion>(Constants::kLightningDashShockExplosionFormID, Constants::kLightningDashVanillaPluginName, "la explosión eléctrica");
			return form;
		}

		// Efectos al empezar: explosiones de polvo y descarga donde está el jugador, modificador de imagen, y el arte
		// y el shader del VisualEffect sobre el jugador durante a_duration segundos (el motor los retira solo).
		void ApplyStartEffects(RE::PlayerCharacter& a_player, float a_duration)
		{
			for (auto* explosion : { GetDustExplosion(), GetShockExplosion() }) {
				if (explosion) {
					(void)a_player.PlaceObjectAtMe(explosion, false);
				}
			}

			if (auto* imageSpaceModifier = GetImageSpaceModifier()) {
				(void)RE::ImageSpaceModifierInstanceForm::Trigger(imageSpaceModifier, 1.0f, nullptr);
			}

			auto* visualEffect = GetVisualEffect();
			if (!visualEffect || a_duration <= 0.0f) {
				return;
			}
			if (visualEffect->data.artObject) {
				(void)a_player.ApplyArtObject(visualEffect->data.artObject, a_duration);
			}
			if (visualEffect->data.effectShader) {
				(void)a_player.ApplyEffectShader(visualEffect->data.effectShader, a_duration);
			}
		}

		// Radio horizontal de a_actor según sus límites (con su escala).
		float GetHorizontalRadius(const RE::Actor& a_actor)
		{
			const auto  boundMin = a_actor.GetBoundMin();
			const auto  boundMax = a_actor.GetBoundMax();
			const float radius = (std::max)({ std::abs(boundMin.x), std::abs(boundMin.y), std::abs(boundMax.x), std::abs(boundMax.y) });
			return radius * a_actor.GetScale();
		}

		// Coloca al jugador y su controlador en a_position sin frenar contra lo que haya en medio,
		// sin velocidad acumulada ni caída (al llegar no hay daño por caída).
		void PlaceWithoutCollision(RE::Actor& a_player, const RE::NiPoint3& a_position)
		{
			a_player.SetPosition(a_position, true);

			if (auto* controller = a_player.GetCharController()) {
				controller->SetLinearVelocityImpl(RE::hkVector4(0.0f, 0.0f, 0.0f, 0.0f));

				// fallStartHeight va en unidades del juego (Z de la posición), no de Havok.
				controller->fallStartHeight = a_position.z;
				controller->fallTime = 0.0f;
			}
		}

		// Termina el desplazamiento: detiene el bucle y devuelve el movimiento.
		void Finish()
		{
			Physics::CancelTickLoop(g_tickToken);
			g_tickToken.reset();
			if (g_active) {
				g_active = false;
				Input::SetMovementLocked(false);
			}
		}
	}

	RE::SpellItem* GetSpell()
	{
		// Aviso una sola vez: CastHook lo consulta en cada lanzamiento de cualquier actor.
		static RE::SpellItem* spell = [] {
			auto* found = LookupForm<RE::SpellItem>(Constants::kLightningDashSpellLocalFormID, Constants::kSoundPluginName);
			if (!found) {
				logs::warn("LightningDash: no se encontró el hechizo (FormID local 0x{:03X}) en \"{}\".",
					Constants::kLightningDashSpellLocalFormID, Constants::kSoundPluginName);
			}
			return found;
		}();
		return spell;
	}

	bool IsOnCooldown(RE::Actor& a_actor)
	{
		auto* effect = GetCooldownEffect();
		auto* magicTarget = a_actor.AsMagicTarget();
		return effect && magicTarget && magicTarget->HasMagicEffect(effect);
	}

	void StartCooldown(RE::Actor& a_actor)
	{
		auto* spell = GetCooldownSpell();
		auto* caster = spell ? a_actor.GetMagicCaster(RE::MagicSystem::CastingSource::kInstant) : nullptr;
		if (caster) {
			caster->CastSpellImmediate(spell, false, &a_actor, 1.0f, false, 0.0f, nullptr);
		}
	}

	RE::NiPoint3 ComputeDestination(RE::Actor& a_player, const RE::NiPoint3& a_weaponPoint, RE::Actor* a_stuckActor, const RE::NiPoint3& a_surfaceNormal)
	{
		if (a_stuckActor) {
			// Delante del actor, del lado por el que llega el jugador, a la altura de sus pies.
			const auto   actorPosition = a_stuckActor->GetPosition();
			RE::NiPoint3 away = a_player.GetPosition() - actorPosition;
			away.z = 0.0f;
			const float awayLength = away.Length();
			away = awayLength > 0.0f ? away / awayLength : RE::NiPoint3{ 0.0f, -1.0f, 0.0f };
			return actorPosition + away * (GetHorizontalRadius(*a_stuckActor) + Constants::kLightningDashActorGap);
		}

		// Separado de la superficie por su normal; en vuelo (normal nula), el punto exacto del arma.
		return a_weaponPoint + a_surfaceNormal * Constants::kLightningDashSurfaceStandoff;
	}

	void Begin(RE::PlayerCharacter& a_player, const RE::NiPoint3& a_destination, std::function<void()> a_onArrived)
	{
		Finish();
		g_active = true;
		const std::uint32_t generation = ++g_generation;

		// Mira hacia el destino, para que el empuje de la animación vaya en la misma dirección.
		const auto start = a_player.GetPosition();
		const auto toDestination = a_destination - start;
		if (toDestination.x != 0.0f || toDestination.y != 0.0f) {
			a_player.SetHeading(std::atan2(toDestination.x, toDestination.y));
		}

		Input::SetMovementLocked(true);

		const float distance = toDestination.Length();
		logs::info("LightningDash: desplazamiento de {:.0f} u hacia ({:.0f}, {:.0f}, {:.0f}).",
			distance, a_destination.x, a_destination.y, a_destination.z);

		ApplyStartEffects(a_player, distance / Constants::kLightningDashSpeed);

		// Estela anclada al pecho (desplazamiento fijo desde los pies, medido al empezar), en el plano vertical del viaje.
		// Va siempre, sin consultar [VFX] Trail; se apaga al terminar el bucle.
		RE::NiPoint3 trailAnchorOffset{ 0.0f, 0.0f, 0.0f };
		if (auto* trailNode = a_player.GetNodeByName(Constants::kLightningDashTrailNodeName)) {
			trailAnchorOffset = trailNode->world.translate - start;
		} else {
			logs::warn("LightningDash: hueso \"{}\" no encontrado, la estela sale desde los pies.", Constants::kLightningDashTrailNodeName);
		}

		RE::NiPoint3 trailUpReference = toDestination.Cross(RE::NiPoint3{ 0.0f, 0.0f, 1.0f });
		const float  trailUpLength = trailUpReference.Length();
		trailUpReference = trailUpLength > 0.0f ? trailUpReference / trailUpLength : RE::NiPoint3{ 0.0f, 0.0f, 1.0f };
		const float trailRoll = Constants::kTrailRollDegrees * std::numbers::pi_v<float> / 180.0f;

		auto trail = std::make_shared<Animation::WeaponTrailGroup>();
		trail->Start(a_player.GetParentCell(), start + trailAnchorOffset, trailUpReference, trailRoll, RE::NiPoint3{ 0.0f, 0.0f, 0.0f }, false);

		// Animación del grito de sprint.
		a_player.NotifyAnimationGraph(Constants::kLightningDashShoutStartEvent);
		(void)Scheduler::After(Constants::kLightningDashSprintEventDelay, [generation]() {
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (player && g_active && g_generation == generation) {
				player->NotifyAnimationGraph(Constants::kLightningDashSprintStartEvent);
			}
		});

		g_tickToken = Physics::StartTickLoop(a_player.GetHandle(), [destination = a_destination, generation, trail, trailAnchorOffset, onArrived = std::move(a_onArrived)](RE::TESObjectREFR& a_refr, float a_deltaSeconds) {
			auto* player = a_refr.As<RE::Actor>();
			if (!player || player->IsDead()) {
				Finish();
				return false;
			}

			const auto  current = player->GetPosition();
			const auto  remaining = destination - current;
			const float remainingDistance = remaining.Length();
			const float step = Constants::kLightningDashSpeed * a_deltaSeconds;
			const bool  arrived = remainingDistance <= step;
			const auto  next = arrived ? destination : current + remaining * (step / remainingDistance);

			PlaceWithoutCollision(*player, next);
			trail->Update(next + trailAnchorOffset, a_deltaSeconds);

			if (!arrived) {
				return true;
			}

			// La llegada recupera el arma (borra referencias y equipa): fuera del tick.
			(void)Scheduler::After(std::chrono::milliseconds{ 0 }, [generation, onArrived]() {
				if (!g_active || g_generation != generation) {
					return;
				}
				Finish();
				if (onArrived) {
					onArrived();
				}
			});
			return false;
		});
	}

	void Cancel()
	{
		Finish();
	}

	bool IsActive() noexcept
	{
		return g_active;
	}

	void RemoveLegacyEffects()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* processLists = RE::ProcessLists::GetSingleton();
		if (!player || !processLists) {
			return;
		}

		auto* visualEffect = GetVisualEffect();
		if (!visualEffect) {
			return;
		}

		auto* art = visualEffect->data.artObject;
		auto* shader = visualEffect->data.effectShader;
		if (!art && !shader) {
			return;
		}

		// Mismo mecanismo que ProcessLists::StopAllMagicEffects, solo para el arte y el shader del VisualEffect.
		const auto handle = player->CreateRefHandle();
		int        removed = 0;
		processLists->ForEachMagicTempEffect([&](RE::BSTempEffect* a_tempEffect) {
			auto* referenceEffect = a_tempEffect->As<RE::ReferenceEffect>();
			if (!referenceEffect || referenceEffect->finished || referenceEffect->target != handle) {
				return RE::BSContainer::ForEachResult::kContinue;
			}

			const auto* modelEffect = a_tempEffect->As<RE::ModelReferenceEffect>();
			const auto* shaderEffect = a_tempEffect->As<RE::ShaderReferenceEffect>();
			if ((art && modelEffect && modelEffect->artObject == art) || (shader && shaderEffect && shaderEffect->effectData == shader)) {
				referenceEffect->finished = true;
				++removed;
			}
			return RE::BSContainer::ForEachResult::kContinue;
		});

		if (removed > 0) {
			logs::info("LightningDash::RemoveLegacyEffects: retirados {} efectos persistentes del VisualEffect del poder.", removed);
		}
	}
}
