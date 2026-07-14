// Implementación del sistema de lanzamiento.
// Controla la creación, activación y seguimiento del arma lanzada.

#include "4.- THROW/ThrowManager.h"

#include "1.- CORE/Constants.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "6.- PHYSICS/PhysicsManager.h"
#include "7.- COMBAT/DamageManager.h"

namespace Throw
{
	namespace
	{
		// Punto de origen del lanzamiento: el nodo del arma en la mano
		// derecha, para que la réplica aparezca en la misma posición que el
		// arma (Mecanica del arma.txt, punto 2), no en la cámara.
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

		RE::NiPoint3 GetCameraForward()
		{
			auto* camera = RE::PlayerCamera::GetSingleton();
			if (!camera || !camera->cameraRoot) {
				return { 0.0f, 1.0f, 0.0f };
			}

			return camera->cameraRoot->world.rotate.GetVectorY();
		}

		// Corrección de paralaje cámara/mano (fallo detectado en la
		// iteración anterior: usar la dirección de la cámara tal cual,
		// aplicada desde el origen en la mano, no converge en el punto al
		// que apunta la mirilla). Se calcula primero el punto real al que
		// apunta la mirilla con un raycast desde la cámara hasta
		// Constants::kMaxThrowDistance, y la dirección de lanzamiento va
		// desde el origen en la mano hacia ese punto — así el origen
		// visual y el punto de impacto coinciden con lo que el jugador ve
		// en la mirilla, sea cual sea la distancia.
		RE::NiPoint3 ComputeAimedDirection(RE::Actor* a_shooter, const RE::NiPoint3& a_origin)
		{
			const auto cameraPos = GetCameraPosition();
			const auto forward = GetCameraForward();
			const auto rayEnd = cameraPos + forward * Constants::kMaxThrowDistance;

			const auto hit = Collision::Raycast(cameraPos, rayEnd, a_shooter);
			const auto aimPoint = hit.hit ? hit.point : rayEnd;

			const auto  direction = aimPoint - a_origin;
			const float length = direction.Length();
			return length > 0.0f ? direction / length : forward;
		}
	}

	void LaunchWeapon(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon, LaunchCallbacks a_callbacks)
	{
		if (!a_shooter || !a_weapon) {
			a_callbacks.onSpawned({});
			return;
		}

		const auto         origin = GetLaunchOrigin(a_shooter);
		const auto         direction = ComputeAimedDirection(a_shooter, origin);
		const RE::NiPoint3 velocity0 = direction * Constants::kThrowInitialSpeed;

		Physics::SpawnReplica(a_shooter, a_weapon, origin, [a_shooter, a_weapon, origin, velocity0, callbacks = a_callbacks](RE::ObjectRefHandle a_handle) {
			callbacks.onSpawned(a_handle);

			if (!a_handle.get()) {
				logs::warn("Throw::LaunchWeapon: la réplica nunca cargó su 3D, se aborta el lanzamiento.");
				return;
			}

			logs::info("Throw::LaunchWeapon: réplica lista, iniciando vuelo parabólico.");

			// Trayectoria parabólica propia (punto 3): posición(t) =
			// origen + velocidad0·t + ½·gravedad·t², sin depender de Havok
			// (la réplica está en modo kKeyframed, sin fuerzas/gravedad
			// del motor). Forma cerrada en vez de acumular velocidad tick
			// a tick, para no arrastrar deriva numérica.
			auto token = Physics::StartTickLoop(a_handle, [a_shooter, a_handle, origin, velocity0, onStuck = callbacks.onStuck, onAutoRecall = callbacks.onAutoRecall, onTickStarted = callbacks.onTickStarted, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				const auto previousPos = a_refr.GetPosition();
				elapsed += a_deltaSeconds;

				RE::NiPoint3 nextPos = origin + velocity0 * elapsed;
				nextPos.z += 0.5f * Constants::kThrowGravity * elapsed * elapsed;

				// Colisión "gruesa" (varios rayos en cruz, ver
				// Collision::SweepRaycast) desde la posición anterior a la
				// siguiente, no solo un punto ni un único rayo fino: a la
				// velocidad del lanzamiento, un rayo infinitamente fino
				// podía pasar de largo junto a geometría irregular o un
				// actor en movimiento, y además clavarse más hundido en la
				// malla (comprobado en el juego). Se ignoran el lanzador y
				// la propia réplica (CFilter no permite excluirlos de la
				// consulta, ver CLAUDE.md).
				const auto hit = Collision::SweepRaycast(previousPos, nextPos, Constants::kThrowCollisionRadius, a_shooter, &a_refr);
				if (hit.hit) {
					auto* actor = hit.target ? hit.target->As<RE::Actor>() : nullptr;

					const auto  travel = nextPos - previousPos;
					const float travelLength = travel.Length();
					const auto  travelDir = travelLength > 0.0f ? travel / travelLength : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };

					// El punto del rayo es donde la línea (infinitamente
					// fina) cruza la superficie golpeada. Contra una
					// superficie normal, el origen del modelo puesto justo
					// ahí deja parte de la malla del arma (con volumen
					// real) hundida dentro de ella, así que se retrocede
					// (comprobado en el juego). Contra un actor es al
					// revés: la capa golpeada (CharController) es una
					// cápsula de colisión más grande que la malla visual
					// real, muy notable en objetivos pequeños — retroceder
					// igual que con una pared deja el arma flotando lejos
					// del cuerpo, así que en vez de eso se avanza
					// (comprobado en el juego).
					const auto stickPoint = actor ?
					                            hit.point + travelDir * Constants::kActorStickForwardOffset :
					                            hit.point - travelDir * Constants::kStickEmbedBackoff;

					a_refr.SetPosition(stickPoint);
					Physics::SyncHavok(a_refr, stickPoint, a_refr.GetAngle());
					logs::info("Throw::LaunchWeapon: impacto en ({:.1f},{:.1f},{:.1f})", hit.point.x, hit.point.y, hit.point.z);

					// Punto 6: contra un actor, no basta con detenerse — hay
					// que aplicar daño/parálisis y seguir su posición
					// mientras el arma siga clavada
					// (Combat::BeginEmbeddedEffect arranca su propio bucle
					// de tick, sustituyendo a este, y decide si llamar a
					// onStuck —clavada de verdad— o a onAutoRecall —objetivo
					// inmune, p. ej. un dragón—). Contra una superficie no
					// hay nada más que hacer.
					if (actor) {
						Combat::BeginEmbeddedEffect(a_shooter, actor, a_handle, onStuck, onAutoRecall, onTickStarted);
					} else {
						onStuck(RE::ActorHandle{});
					}

					return false;
				}

				// El agua no es una superficie donde clavarse (caso no
				// cubierto por el documento): se trata igual que no
				// impactar contra nada (punto 5).
				if (a_refr.IsInWater()) {
					logs::info("Throw::LaunchWeapon: ha caído al agua, recuperando automáticamente.");
					onAutoRecall();
					return false;
				}

				if ((nextPos - origin).Length() >= Constants::kMaxThrowDistance) {
					logs::info("Throw::LaunchWeapon: distancia máxima superada sin impactar, recuperando automáticamente.");
					onAutoRecall();
					return false;
				}

				a_refr.SetPosition(nextPos);
				Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());
				return true;
			});

			callbacks.onTickStarted(token);
		});
	}
}
