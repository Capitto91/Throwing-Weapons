// Implementación del sistema de retorno.
// Controla velocidad, duración máxima del viaje y sincronización con la mano.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "5.- RETURN/ReturnTrajectory.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponTrail.h"
#include "9.- MATH/CurveMath.h"

#include <algorithm>
#include <vector>

namespace Return
{
	namespace
	{
		// Nodo de la mano derecha (mismo origen que Throw::GetLaunchOrigin
		// para el lanzamiento, ver 4.- THROW/ThrowManager): destino real
		// hacia el que vuela el arma, no la posición del actor.
		RE::NiPoint3 GetHandPosition(RE::Actor* a_player)
		{
			if (auto* handNode = a_player->GetNodeByName("WEAPON")) {
				return handNode->world.translate;
			}

			return a_player->GetPosition();
		}

		// Cuerpo real de BeginReturn (trayectoria curva + aceleración
		// híbrida, puntos 7-8), separado para que el temblor de
		// desprendimiento (punto 11, ver BeginReturn más abajo) pueda
		// insertarse antes sin duplicar esta lógica: se invoca tal cual al
		// terminar el temblor si la réplica venía clavada, o de inmediato
		// si venía en vuelo.
		void BeginReturnMovement(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, ReturnCallbacks a_callbacks)
		{
			auto replica = a_replicaHandle.get();
			if (!a_player || !replica) {
				logs::warn("Return::BeginReturnMovement: sin jugador o réplica válida, se aborta el regreso.");
				a_callbacks.onArrived();
				return;
			}

			const auto start = replica->GetPosition();
			const auto initialHandPos = GetHandPosition(a_player);

			const float initialDistance = (initialHandPos - start).Length();
			const float acceleration = ComputeReturnAcceleration(initialDistance);
			const auto  controlPoint = ComputeReturnControlPoint(start, initialHandPos, GetPlayerRightVector(a_player), Constants::kReturnCurveAnchorFraction);

			logs::info(
				"Return::BeginReturnMovement: distancia inicial {:.1f}, aceleración {:.1f}",
				initialDistance, acceleration);

			// Estela visual (ver PLAN-trail.md): instancia propia del
			// regreso, independiente de la de la ida (esa ya se destruyó al
			// cancelarse su bucle de tick antes de llegar aquí, ver
			// WeaponManager::BeginReturn). Mismo motivo que en
			// Throw::LaunchWeapon para el std::shared_ptr, ver WeaponTrail.h.
			auto trail = std::make_shared<Animation::WeaponTrail>();
			if (auto* node3D = replica->Get3D()) {
				trail->Start(replica->GetParentCell(), Animation::GetVisualTransform(*node3D));
			}

			auto token = Physics::StartTickLoop(a_replicaHandle, [a_player, start, controlPoint, initialDistance, acceleration, onArrived = a_callbacks.onArrived, elapsed = 0.0f, hitActors = std::vector<RE::ActorHandle>{}, trail, loggedHandAxisDiagnostic = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				const auto previousPos = a_refr.GetPosition();
				elapsed += a_deltaSeconds;

				// Punto 10: se calcula y escribe el giro a mano cada tick
				// (ver Animation::TickSpin), igual que en la ida.
				Animation::TickSpin(a_refr, elapsed);

				// El extremo final de la curva se recalcula cada tick (a
				// diferencia del inicio y el punto de control, fijados una
				// única vez): así el regreso no pierde de vista al jugador si
				// se mueve mientras el arma vuela de vuelta.
				const auto handPos = GetHandPosition(a_player);

				// Mejora Kratos #4, campo 2 (diagnóstico, todavía sin usar en
				// la curva): antes de confiar en la rotación del hueso "WEAPON"
				// para el punto de control p2 (sin precedente en este proyecto
				// de leer la *rotación* de ese hueso, solo su posición), se
				// registran los tres ejes una vez por regreso para comprobar a
				// ojo cuál apunta de verdad "hacia fuera de la palma" -- no
				// asumir que es Y por analogía con la cámara (ver
				// PLAN-mejoras-kratos.md).
				if (!loggedHandAxisDiagnostic) {
					if (auto* handNode = a_player->GetNodeByName("WEAPON")) {
						const auto vecX = handNode->world.rotate.GetVectorX();
						const auto vecY = handNode->world.rotate.GetVectorY();
						const auto vecZ = handNode->world.rotate.GetVectorZ();
						logs::info(
							"Return::BeginReturnMovement: diagnóstico ejes hueso mano -- X=({:.2f},{:.2f},{:.2f}) Y=({:.2f},{:.2f},{:.2f}) Z=({:.2f},{:.2f},{:.2f})",
							vecX.x, vecX.y, vecX.z, vecY.x, vecY.y, vecY.z, vecZ.x, vecZ.y, vecZ.z);
					}
					loggedHandAxisDiagnostic = true;
				}

				const float traveled = ComputeTraveledDistance(acceleration, elapsed);
				const float t = initialDistance > 0.0f ? std::clamp(traveled / initialDistance, 0.0f, 1.0f) : 1.0f;

				const auto nextPos = Math::EvaluateQuadraticBezier(start, controlPoint, handPos, t);

				// Punto 9: golpear a un enemigo durante el regreso ya no se
				// queda clavado, el vuelo por la curva continúa igual — solo
				// se aplica el golpe (stagger + daño reducido, ver
				// Combat::ApplyReturnHit) y se sigue. Un impacto contra algo
				// que no sea un actor (pared, suelo...) se ignora sin más: a
				// diferencia de la ida, el regreso no choca contra el
				// escenario. Cada actor solo recibe el golpe una vez por
				// regreso (hitActors), para no repetirlo tick a tick mientras
				// la réplica pasa cerca de él.
				const auto hit = Collision::SweepRaycast(previousPos, nextPos, Constants::kThrowCollisionRadius, a_player, &a_refr);
				if (auto* actor = hit.hit && hit.target ? hit.target->As<RE::Actor>() : nullptr) {
					RE::ActorHandle actorHandle(actor);
					if (std::ranges::find(hitActors, actorHandle) == hitActors.end()) {
						hitActors.push_back(actorHandle);
						Combat::ApplyReturnHit(a_player, actor);
					}
				}

				a_refr.SetPosition(nextPos);
				Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());
				if (auto* node3D = a_refr.Get3D()) {
					trail->Update(Animation::GetVisualTransform(*node3D), a_deltaSeconds);
				}

				if ((handPos - nextPos).Length() <= Constants::kReturnArrivalDistance) {
					logs::info("Return::BeginReturnMovement: la réplica ha llegado a la mano.");
					onArrived();
					return false;
				}

				return true;
			});

			a_callbacks.onTickStarted(token);
		}
	}

	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, bool a_wasStuck, ReturnCallbacks a_callbacks)
	{
		if (!a_player || !a_replicaHandle.get()) {
			logs::warn("Return::BeginReturn: sin jugador o réplica válida, se aborta el regreso.");
			a_callbacks.onArrived();
			return;
		}

		if (!a_wasStuck) {
			BeginReturnMovement(a_player, a_replicaHandle, std::move(a_callbacks));
			return;
		}

		// Punto 11: temblor de desprendimiento antes de iniciar el
		// movimiento de vuelta -- sin mover la réplica (posición y Havok
		// intactos), solo escribe una oscilación de frecuencia y amplitud
		// crecientes en el nodo de giro visual (ver Animation::TickShudder).
		// Al agotarse Constants::kStickShudderDuration, este mismo bucle de
		// tick arranca BeginReturnMovement y termina el suyo devolviendo
		// false -- WeaponState solo ve un token activo cada vez (primero el
		// de este temblor, luego el del movimiento), porque ambos pasan por
		// el mismo a_callbacks.onTickStarted.
		//
		// baseRotation: la rotación del nodo de giro justo en el instante
		// de clavarse (viene de un ángulo de vuelo arbitrario, congelado
		// desde que Throw::LaunchWeapon dejó de llamar a TickSpin sobre esta
		// réplica) -- capturada una única vez aquí para que TickShudder
		// oscile sobre ella en vez de sustituirla por una rotación absoluta
		// desde cero, que producía un salto visual perceptible al empezar
		// el temblor (reportado por el usuario como "cambia de posición").
		RE::NiMatrix3 baseRotation;
		if (auto replica = a_replicaHandle.get()) {
			if (auto* root = replica->Get3D()) {
				if (auto* spinNode = root->GetObjectByName(Constants::kWeaponSpinNodeName)) {
					baseRotation = spinNode->local.rotate;
				}
			}
		}

		logs::info("Return::BeginReturn: arma clavada, temblor de desprendimiento ({:.2f}s) antes de regresar.", Constants::kStickShudderDuration);

		auto shudderToken = Physics::StartTickLoop(a_replicaHandle, [a_player, a_replicaHandle, callbacks = a_callbacks, baseRotation, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			elapsed += a_deltaSeconds;

			if (elapsed >= Constants::kStickShudderDuration) {
				BeginReturnMovement(a_player, a_replicaHandle, std::move(callbacks));
				return false;
			}

			// TickShudder solo escribe la rotación local del nodo de giro;
			// a diferencia del vuelo (donde Physics::SyncHavok ya llama a
			// Update3DPosition en cada tick porque la posición también
			// cambia), aquí la réplica no se mueve, así que sin esta
			// llamada el motor nunca propaga ese cambio a world.rotate y el
			// temblor no llega a verse aunque el cálculo sea correcto.
			Animation::TickShudder(a_refr, baseRotation, elapsed);
			a_refr.Update3DPosition(true);
			return true;
		});

		a_callbacks.onTickStarted(shudderToken);
	}
}
