// Poder Lightning Dash -- ver LightningDash.h.

#include "3.- WEAPON/LightningDash.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/FrameHook.h"
#include "1.- CORE/Scheduler.h"
#include "2.- INPUT/InputManager.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponImpactVFX.h"
#include "8.- ANIMATION/WeaponTrailGroup.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <numbers>
#include <vector>

namespace Weapon::LightningDash
{
	namespace
	{
		// Estado del desplazamiento, solo hilo principal. g_generation descarta llegadas de uno cancelado.
		Physics::TickToken g_tickToken;
		bool               g_active = false;
		std::uint32_t      g_generation = 0;

		// Golpe en salto en curso (solo hilo principal).
		struct SlamState
		{
			bool         active{ false };
			bool         animationPlaying{ false };  // attackStart aceptado: el impacto lo marca la anotación
			bool         landed{ false };
			bool         impactDone{ false };
			RE::NiPoint3 ground{};
			double       startTime{ 0.0 };  // FrameHook::Now del attackStart
		};
		SlamState g_slam;

		// Tiempo hasta la anotación del golpe: mediana de g_slamLeadSamples (nominal al cargar).
		float              g_slamLeadSeconds = Constants::kSlamAnimationLeadTime;
		std::vector<float> g_slamLeadSamples;

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

		// Capas que cuentan como suelo para el golpe en salto: terreno, estructuras y props grandes.
		// Actores, árboles, objetos sueltos, escombros o armas se atraviesan.
		bool IsGroundLayer(RE::COL_LAYER a_layer)
		{
			switch (a_layer) {
			case RE::COL_LAYER::kStatic:
			case RE::COL_LAYER::kAnimStatic:
			case RE::COL_LAYER::kTerrain:
			case RE::COL_LAYER::kGround:
			case RE::COL_LAYER::kProps:
				return true;
			default:
				return false;
			}
		}

		// Suelo bajo a_from hasta a_maxDrop, atravesando a_skip (la réplica) y lo que no sea suelo (IsGroundLayer).
		std::optional<RE::NiPoint3> FindGroundBelow(const RE::NiPoint3& a_from, float a_maxDrop, RE::Actor& a_player, RE::TESObjectREFR* a_skip)
		{
			constexpr int   kMaxSkips = 8;
			constexpr float kSkipStep = 5.0f;

			RE::NiPoint3       from = a_from;
			const RE::NiPoint3 to = a_from - RE::NiPoint3{ 0.0f, 0.0f, a_maxDrop };

			for (int i = 0; i <= kMaxSkips && from.z > to.z; ++i) {
				const auto hit = Collision::Raycast(from, to, &a_player);
				if (!hit.hit) {
					return std::nullopt;
				}
				if ((a_skip && hit.target == a_skip) || !IsGroundLayer(hit.layer)) {
					from = hit.point - RE::NiPoint3{ 0.0f, 0.0f, kSkipStep };
					continue;
				}
				return hit.point;
			}
			return std::nullopt;
		}

		// Termina el desplazamiento (y el golpe en salto): detiene el bucle, apaga el Global del golpe y devuelve el movimiento.
		void Finish()
		{
			Physics::CancelTickLoop(g_tickToken);
			g_tickToken.reset();
			if (g_slam.active) {
				g_slam = {};
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					Animation::SetSlamTrigger(*player, false);
				}
			}
			if (g_active) {
				g_active = false;
				Input::SetMovementLocked(false);
			}
		}

		// La animación del golpe ha empezado: la bajada dura lo medido hasta la anotación, con red de seguridad
		// por si no llega.
		float OnSlamAnimationStarted(std::uint32_t a_generation)
		{
			g_slam.animationPlaying = true;
			g_slam.startTime = FrameHook::Now();

			(void)Scheduler::After(Constants::kSlamReleaseFallbackWindow, [a_generation]() {
				if (g_slam.active && !g_slam.impactDone && g_generation == a_generation) {
					logs::warn("LightningDash: la anotación del golpe en salto no llegó (red de seguridad). Revisa que el submod Slam de Open Animation Replacer esté activo.");
					OnSlamImpactAnimationEvent(false);
				}
			});
			return g_slamLeadSeconds;
		}

		// Sin ataque en curso. Dentro de otro (p. ej. Throw.hkx sin terminar), attackStart se rechaza o encadena el
		// siguiente golpe del combo, que es otro clip sin la anotación del golpe en salto.
		bool IsAttackIdle(RE::Actor& a_actor)
		{
			return a_actor.AsActorState()->GetAttackState() == RE::ATTACK_STATE_ENUM::kNone;
		}

		// Golpe en salto desde la posición actual hasta a_ground: Global + attackStart (OAR pone el clip) y bajada
		// en línea recta que dura lo que tarda el clip en llegar a su anotación, para tocar el suelo en el golpe.
		// Hasta que no haya ataque en curso y el grafo acepte el golpe, el jugador espera suspendido y se reintenta
		// cada tick hasta kSlamStartTimeoutSeconds; sin animación, baja a kLightningDashSpeed y el impacto es al tocar el suelo.
		void BeginSlam(const RE::NiPoint3& a_ground, std::uint32_t a_generation)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player) {
				Finish();
				return;
			}

			const auto  top = player->GetPosition();
			const float height = top.z - a_ground.z;

			g_slam = {};
			g_slam.active = true;
			g_slam.ground = a_ground;

			// iRightHandType a una mano: el arma recuperada todavía se está equipando (como en Atrape).
			// Sin el Global no se envía attackStart: saldría el ataque ligero normal en vez del golpe.
			const bool hasTrigger = Animation::SetSlamTrigger(*player, true);
			bool       waitingForAnimation = false;
			float      descentTime = height / Constants::kLightningDashSpeed;
			if (hasTrigger) {
				player->SetGraphVariableInt(Constants::kRightHandTypeGraphVariable, Constants::kRightHandTypeOneHanded);
				if (IsAttackIdle(*player) && player->NotifyAnimationGraph(Constants::kLightAttackAnimationEvent)) {
					descentTime = OnSlamAnimationStarted(a_generation);
				} else {
					waitingForAnimation = true;
				}
			} else {
				logs::warn("LightningDash: Global '{}' no encontrado, golpe en salto sin animación.", Constants::kSlamTriggerGlobalEditorID);
			}

			if (waitingForAnimation) {
				logs::info("LightningDash: golpe en salto desde {:.0f} u de altura, esperando a que termine el ataque en curso y el grafo acepte el golpe.", height);
			} else {
				logs::info("LightningDash: golpe en salto desde {:.0f} u de altura, bajada en {:.2f} s.", height, descentTime);
			}

			g_tickToken = Physics::StartTickLoop(player->GetHandle(), [top, height, ground = a_ground, descentTime, waitingForAnimation, a_generation, waited = 0.0f, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				auto* actor = a_refr.As<RE::Actor>();
				if (!actor || actor->IsDead()) {
					Finish();
					return false;
				}

				// Suspendido en la llegada mientras se reintenta el ataque.
				if (waitingForAnimation) {
					PlaceWithoutCollision(*actor, top);
					waited += a_deltaSeconds;
					if (IsAttackIdle(*actor) && actor->NotifyAnimationGraph(Constants::kLightAttackAnimationEvent)) {
						waitingForAnimation = false;
						descentTime = OnSlamAnimationStarted(a_generation);
						logs::info("LightningDash: ataque del golpe aceptado tras {:.2f} s, bajada en {:.2f} s.", waited, descentTime);
					} else if (waited >= Constants::kSlamStartTimeoutSeconds) {
						waitingForAnimation = false;
						Animation::SetSlamTrigger(*actor, false);
						logs::warn("LightningDash: el grafo no aceptó '{}' en {:.2f} s, golpe en salto sin animación.", Constants::kLightAttackAnimationEvent, waited);
						descentTime = height / Constants::kLightningDashSpeed;
					}
					return true;
				}

				elapsed += a_deltaSeconds;
				const float fraction = descentTime > 0.0f ? (std::min)(elapsed / descentTime, 1.0f) : 1.0f;
				PlaceWithoutCollision(*actor, top + (ground - top) * fraction);
				if (fraction < 1.0f) {
					return true;
				}

				// En el suelo. Con animación, el impacto espera a la anotación; sin ella es ahora (fuera del tick: coloca la explosión).
				g_slam.landed = true;
				if (!g_slam.animationPlaying) {
					(void)Scheduler::After(std::chrono::milliseconds{ 0 }, [a_generation]() {
						if (g_slam.active && g_generation == a_generation) {
							OnSlamImpactAnimationEvent(false);
						}
					});
				}
				return false;
			});
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

	std::optional<RE::NiPoint3> FindSlamGround(RE::Actor& a_player, const RE::NiPoint3& a_destination, RE::TESObjectREFR* a_replica)
	{
		// Sin suelo a menos de kLightningDashSlamMaxHeight no hay golpe.
		return FindGroundBelow(a_destination, Constants::kLightningDashSlamMaxHeight, a_player, a_replica);
	}

	void Begin(RE::PlayerCharacter& a_player, const RE::NiPoint3& a_destination, std::optional<RE::NiPoint3> a_slamGround, std::function<void()> a_onArrived)
	{
		Finish();
		g_active = true;
		const std::uint32_t generation = ++g_generation;

		// Mira hacia el destino.
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

		g_tickToken = Physics::StartTickLoop(a_player.GetHandle(), [destination = a_destination, slamGround = a_slamGround, generation, trail, trailAnchorOffset, onArrived = std::move(a_onArrived)](RE::TESObjectREFR& a_refr, float a_deltaSeconds) {
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
			(void)Scheduler::After(std::chrono::milliseconds{ 0 }, [generation, slamGround, onArrived]() {
				if (!g_active || g_generation != generation) {
					return;
				}

				if (!slamGround) {
					Finish();
					if (onArrived) {
						onArrived();
					}
					return;
				}

				// Golpe en salto: el arma vuelve a la mano y empieza el golpe, sin soltar el control del jugador.
				Physics::CancelTickLoop(g_tickToken);
				g_tickToken.reset();
				if (onArrived) {
					onArrived();
				}
				BeginSlam(*slamGround, generation);
			});
			return false;
		});
	}

	void OnSlamImpactAnimationEvent(bool a_fromAnnotation)
	{
		if (!g_slam.active || g_slam.impactDone) {
			return;
		}
		g_slam.impactDone = true;

		// Tiempo del clip hasta su anotación, para la bajada del siguiente golpe (mediana, como en el Atrape).
		if (a_fromAnnotation && g_slam.animationPlaying) {
			const float measured = static_cast<float>(FrameHook::Now() - g_slam.startTime);
			const float nominal = Constants::kSlamAnimationLeadTime;
			if (measured >= nominal * Constants::kSlamLeadMeasureMinFactor && measured <= nominal * Constants::kSlamLeadMeasureMaxFactor) {
				g_slamLeadSamples.push_back(measured);
				if (g_slamLeadSamples.size() > Constants::kSlamLeadSampleCount) {
					g_slamLeadSamples.erase(g_slamLeadSamples.begin());
				}

				std::vector<float> sorted = g_slamLeadSamples;
				std::ranges::sort(sorted);
				const std::size_t middle = sorted.size() / 2;
				g_slamLeadSeconds = sorted.size() % 2 != 0 ? sorted[middle] : 0.5f * (sorted[middle - 1] + sorted[middle]);
			} else {
				logs::warn("LightningDash: medida del golpe en salto fuera de rango ({:.3f} s), se conserva {:.3f} s.", measured, g_slamLeadSeconds);
			}
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			Finish();
			return;
		}

		// Si la anotación llega antes de tocar el suelo, la bajada termina ahora.
		Physics::CancelTickLoop(g_tickToken);
		g_tickToken.reset();
		if (!g_slam.landed) {
			PlaceWithoutCollision(*player, g_slam.ground);
		}

		// Explosión solo con la anotación, que prueba que se ha visto Slam.hkx; sin ella (red de seguridad o sin
		// animación) solo hay bajada. El jugador como propietario, para que su propia explosión no le alcance.
		if (a_fromAnnotation) {
			Animation::SpawnSlamVFX(*player, g_slam.ground);
		}
		Animation::SetSlamTrigger(*player, false);

		if (!g_slam.animationPlaying) {
			Finish();
			return;
		}

		// attackStop pasada la cola del clip: el submod solo procesa sus anotaciones y el grafo no vuelve solo a reposo.
		const std::uint32_t generation = g_generation;
		(void)Scheduler::After(Constants::kSlamAnimationTailDuration, [generation]() {
			if (!g_active || g_generation != generation) {
				return;
			}
			if (auto* player = RE::PlayerCharacter::GetSingleton()) {
				player->NotifyAnimationGraph(Constants::kAttackStopAnimationEvent);
			}
			Finish();
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
