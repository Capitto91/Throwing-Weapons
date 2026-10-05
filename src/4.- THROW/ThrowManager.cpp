// Lanzamiento -- ver ThrowManager.h.

#include "4.- THROW/ThrowManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "1.- CORE/Settings.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "11.- SKYRIM/TDMBridge.h"
#include "12.- AUDIO/SoundResolver.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponGlow.h"
#include "8.- ANIMATION/WeaponImpactVFX.h"
#include "8.- ANIMATION/WeaponTrailGroup.h"
#include "9.- MATH/RotationMath.h"

#include <cmath>
#include <numbers>

namespace Throw
{
	namespace
	{
		// Origen del lanzamiento: el nodo del arma en la mano derecha.
		RE::NiPoint3 GetLaunchOrigin(RE::Actor* a_shooter)
		{
			if (auto* weaponNode = a_shooter->GetNodeByName("WEAPON")) {
				return weaponNode->world.translate;
			}

			return a_shooter->GetPosition();
		}

		RE::NiPoint3 GetCameraPosition()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			return camera && camera->cameraRoot ? camera->cameraRoot->world.translate : RE::NiPoint3{};
		}

		// Dirección de la retícula. En primera persona cameraRoot va unos 4,5° más inclinado hacia abajo que la
		// mirada (medido en el juego), así que se usa el ángulo del jugador, que es lo que sigue la retícula.
		RE::NiPoint3 GetCameraForward()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera || !camera->cameraRoot) {
				return { 0.0f, 1.0f, 0.0f };
			}

			if (auto* player = RE::PlayerCharacter::GetSingleton(); player && ActorUtils::IsPlayerInFirstPerson()) {
				// Rumbo 0 hacia +Y, positivo hacia +X; inclinación positiva hacia abajo.
				const float pitch = player->GetAngleX();
				const float heading = player->GetAngleZ();
				return { std::sin(heading) * std::cos(pitch), std::cos(heading) * std::cos(pitch), -std::sin(pitch) };
			}

			return camera->cameraRoot->world.rotate.GetVectorY();
		}

		// Dirección en línea recta desde la mano hacia el punto bajo la mirilla (raycast desde la cámara).
		RE::NiPoint3 ComputeAimedDirection(RE::Actor* a_shooter, const RE::NiPoint3& a_origin)
		{
			const auto cameraPos = GetCameraPosition();
			const auto forward = GetCameraForward();
			const auto rayEnd = cameraPos + forward * Constants::kAimRaycastDistance;

			const auto hit = Collision::Raycast(cameraPos, rayEnd, a_shooter);
			const auto aimPoint = hit.hit ? hit.point : rayEnd;

			const RE::NiPoint3 toAimPoint = aimPoint - a_origin;
			const float        length = toAimPoint.Length();
			return length > 0.0f ? toAimPoint / length : forward;
		}

		// Gravedad Z del mundo de Havok en unidades de juego (kThrowFallbackWorldGravity si no se lee).
		float GetWorldGravity(RE::Actor* a_shooter)
		{
			auto* cell = a_shooter->GetParentCell();
			auto* bhkWorld = cell ? cell->GetbhkWorld() : nullptr;
			auto* world = bhkWorld ? bhkWorld->GetWorld1() : nullptr;
			const float scale = RE::bhkWorld::GetWorldScale();

			if (!world || scale <= 0.0f) {
				logs::warn("Throw::GetWorldGravity: sin mundo de Havok accesible, se usa el valor de respaldo {:.3f}.", Constants::kThrowFallbackWorldGravity);
				return Constants::kThrowFallbackWorldGravity;
			}

			alignas(16) float components[4];
			_mm_store_ps(components, world->gravity.quad);
			const float gravity = components[2] / scale;

			if (!std::isfinite(gravity) || gravity >= 0.0f) {
				logs::warn("Throw::GetWorldGravity: valor leído no válido ({}), se usa el valor de respaldo {:.3f}.", gravity, Constants::kThrowFallbackWorldGravity);
				return Constants::kThrowFallbackWorldGravity;
			}
			return gravity;
		}

		// Caída por gravedad constante: ½·gravedad·t².
		float ComputeGravityDrop(float a_elapsed, float a_gravity)
		{
			return 0.5f * a_gravity * a_elapsed * a_elapsed;
		}
	}

	void LaunchWeapon(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon, LaunchCallbacks a_callbacks)
	{
		if (!a_shooter || !a_weapon) {
			a_callbacks.onSpawned({});
			return;
		}

		const auto  origin = GetLaunchOrigin(a_shooter);
		// Velocidad y gravedad de Settings, fijas durante el vuelo.
		const float speed = Settings::GetThrowSpeed();
		const float gravity = GetWorldGravity(a_shooter) * Settings::GetThrowGravityMult();

		// Con target lock de TDM se apunta al torso del objetivo con predicción; si no, a la mirilla.
		RE::NiPoint3 velocity0;
		const auto   lockedTarget = a_shooter == RE::PlayerCharacter::GetSingleton() ? TDMBridge::GetLockedTarget() : nullptr;
		const auto   targetPoint = lockedTarget ? TDMBridge::GetTargetPoint(*lockedTarget) : RE::NiPoint3{};
		// Parte de la línea recta al objetivo a la velocidad configurada; sin distancia, apuntado normal.
		if (lockedTarget && (targetPoint - origin).Length() > 1.0f) {
			RE::NiPoint3 targetVelocity;
			lockedTarget->GetLinearVelocity(targetVelocity);

			velocity0 = targetPoint - origin;
			velocity0.Unitize();
			velocity0 *= speed;
			// Sin solución exacta velocity0 queda apuntando a la posición futura estimada.
			(void)TDMBridge::PredictAimProjectile(origin, targetPoint, targetVelocity, -gravity, velocity0);
		} else {
			velocity0 = ComputeAimedDirection(a_shooter, origin) * speed;
		}

		// Silbido de lanzamiento al instante, sin esperar al 3D de la réplica.
		Audio::PlayFileOneShot(origin, Constants::kThrowLaunchSoundFilePath, Constants::kSoundHandleVolume);

		// Rotación del arma equipada justo antes de convertirse en réplica: base del giro.
		const RE::NiMatrix3 capturedWeaponWorldRotation = Animation::GetEquippedWeaponWorldRotation(*a_shooter);

		Physics::SpawnReplica(a_shooter, a_weapon, origin, [a_shooter, a_weapon, origin, velocity0, gravity, capturedWeaponWorldRotation, callbacks = a_callbacks](RE::ObjectRefHandle a_handle) {
			callbacks.onSpawned(a_handle);

			if (!a_handle.get()) {
				logs::warn("Throw::LaunchWeapon: la réplica nunca cargó su 3D, se aborta el lanzamiento.");
				return;
			}


			// Rotación local sobre el nodo raíz que reproduce la pose capturada; TickSpin gira sobre ella.
			RE::NiMatrix3 launchBaseLocal;
			// Estela del tramo de ida, movida por el bucle de tick de abajo.
			auto trail = std::make_shared<Animation::WeaponTrailGroup>();
			if (auto replica = a_handle.get()) {
				// Plano de la estela: normal del plano de la parábola más el roll fijo Constants::kTrailRollDegrees.
				RE::NiPoint3 trailUpReference = velocity0.Cross(RE::NiPoint3{ 0.0f, 0.0f, 1.0f });
				const float  trailUpLength = trailUpReference.Length();
				trailUpReference = trailUpLength > 0.0f ? trailUpReference / trailUpLength : RE::NiPoint3{ 0.0f, 0.0f, 1.0f };

				const float trailRoll = Constants::kTrailRollDegrees * std::numbers::pi_v<float> / 180.0f;

				// Anclaje de la estela: offset desde el nodo raíz (base del mango) rotado con rootWorld.
				RE::NiPoint3 trailAnchorWorldOffset{ 0.0f, 0.0f, 0.0f };

				if (auto* node3D = replica->Get3D()) {
					const RE::NiMatrix3 rootWorld = node3D->world.rotate;
					launchBaseLocal = Math::LocalRotationFromWorld(rootWorld, capturedWeaponWorldRotation);
					trailAnchorWorldOffset = rootWorld * Constants::kTrailAnchorLocalOffset;

					// Primer giro a elapsed=0 para no mostrar la pose de reposo del NIF.
					Animation::TickSpin(*replica, 0.0f, launchBaseLocal);
				}

				trail->Start(replica->GetParentCell(), replica->GetPosition(), trailUpReference, trailRoll, trailAnchorWorldOffset);
			}

			// Parábola en forma cerrada: origen + velocidad0·t + ½·gravedad·t².
			auto token = Physics::StartTickLoop(a_handle, [a_shooter, a_handle, origin, velocity0, gravity, launchBaseLocal, trail, onStuck = callbacks.onStuck, onAutoRecall = callbacks.onAutoRecall, onTickStarted = callbacks.onTickStarted, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				const auto previousPos = a_refr.GetPosition();
				elapsed += a_deltaSeconds;

				// Giro por código cada tick sobre launchBaseLocal.
				Animation::TickSpin(a_refr, elapsed, launchBaseLocal);

				const float gravityDrop = ComputeGravityDrop(elapsed, gravity);

				RE::NiPoint3 nextPos = origin + velocity0 * elapsed;
				nextPos.z += gravityDrop;

				// Colisión con varios rayos en cruz desde la posición anterior, ignorando lanzador y réplica.
				const auto hit = Collision::SweepRaycast(previousPos, nextPos, Constants::kThrowCollisionRadius, a_shooter, &a_refr);
				if (hit.hit) {
					auto* actor = hit.target ? hit.target->As<RE::Actor>() : nullptr;

					const auto  travel = nextPos - previousPos;
					const float travelLength = travel.Length();
					const auto  travelDir = travelLength > 0.0f ? travel / travelLength : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };

					// Ajuste del punto de clavado: retrocede contra superficies y avanza contra actores.
					const auto stickPoint = actor ?
					                            hit.point + travelDir * Constants::kActorStickForwardOffset :
					                            hit.point - travelDir * Constants::kStickEmbedBackoff;

					a_refr.SetPosition(stickPoint);
					Physics::SyncHavok(a_refr, stickPoint, a_refr.GetAngle());
					trail->Update(stickPoint, a_deltaSeconds);

					// Explosión de impacto en la cabeza del martillo, diferida un tick con Scheduler
					// (PlaceObjectAtMe dentro del tick cuelga el juego).
					const auto impactVfxPosition = a_refr.Get3D() ? Animation::GetGlowAnchorPosition(a_refr.Get3D()) : stickPoint;
					// (void): disparo suelto, nada cancela esto desde fuera.
					(void)Scheduler::After(Constants::kTickInterval, [a_shooter, impactVfxPosition]() {
						if (a_shooter) {
							Animation::SpawnImpactVFX(*a_shooter, impactVfxPosition);
						}
					});

					// Descarga eléctrica del impacto, diferida un tick; no se coloca si el arma se desclava antes.
					const auto hazardGeneration = Combat::GetHazardGeneration();
					if (actor) {
						(void)Scheduler::After(Constants::kTickInterval, [a_shooter, targetHandle = RE::ActorHandle(actor), hazardGeneration]() {
							if (auto target = targetHandle.get(); target && a_shooter) {
								Combat::SpawnActorHazard(a_shooter, *target, hazardGeneration);
							}
						});
					} else {
						(void)Scheduler::After(Constants::kTickInterval, [a_shooter, point = hit.point, normal = hit.normal, hazardGeneration]() {
							if (a_shooter) {
								Combat::SpawnSurfaceHazard(a_shooter, *a_shooter, point, normal, hazardGeneration);
							}
						});
					}

					// Contra un actor, Combat::BeginEmbeddedEffect aplica el golpe y sigue al objetivo;
					// contra una superficie queda clavada aquí.
					if (actor) {
						Combat::BeginEmbeddedEffect(a_shooter, actor, a_handle, onStuck, onAutoRecall, onTickStarted);
					} else {
						onStuck(RE::ActorHandle{}, hit.normal);
					}

					return false;
				}

				// El agua no clava el arma: se detiene y avisa por onWaterImpact.
				if (a_refr.IsInWater()) {
					logs::info("Throw::LaunchWeapon: ha caído al agua, recuperando automáticamente.");
					onAutoRecall();
					return false;
				}

				a_refr.SetPosition(nextPos);
				Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());
				trail->Update(nextPos, a_deltaSeconds);
				return true;
			});

			callbacks.onTickStarted(token);
		});
	}
}
