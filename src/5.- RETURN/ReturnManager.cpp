// Implementación del sistema de retorno.
// Controla velocidad, duración máxima del viaje y sincronización con la mano.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/CatchSound.h"
#include "12.- AUDIO/FlightSound.h"
#include "5.- RETURN/ReturnTrajectory.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
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

			// Duración estimada del regreso, solo para el log -- ya no arma
			// ningún disparo (ver Audio::CatchSound, que se reajusta cada
			// tick con la distancia real en vez de una predicción única).
			const float estimatedDuration = ComputeReturnDuration(acceleration, initialDistance);

			logs::info(
				"Return::BeginReturnMovement: distancia inicial {:.1f}, aceleración {:.1f}, duración estimada {:.2f}s",
				initialDistance, acceleration, estimatedDuration);

			// Sonido (ver 12.- AUDIO/FlightSound): mismo criterio que
			// Throw::LaunchWeapon -- silbido suelto al iniciar el tramo de
			// movimiento del regreso más el loop posicional que sigue a la
			// réplica mientras dura la curva de vuelta.
			auto flightSound = std::make_shared<Audio::FlightSound>();
			// Sonido de atrape sincronizado en tiempo real (ver
			// 12.- AUDIO/CatchSound) -- no tiene ningún "Start" propio, se
			// arranca solo dentro de su primer Update() cuando el tiempo
			// restante estimado lo justifique.
			auto catchSound = std::make_shared<Audio::CatchSound>();
			if (auto* node3D = replica->Get3D()) {
				Audio::PlaySoundOneShot(start, Constants::kThrowLaunchSoundLocalFormID);
				flightSound->Start(node3D, Constants::kFlightLoopSoundLocalFormID);
			}

			auto token = Physics::StartTickLoop(a_replicaHandle, [a_player, start, controlPoint, initialDistance, acceleration, onArrived = a_callbacks.onArrived, elapsed = 0.0f, progressElapsed = 0.0f, hitActors = std::vector<RE::ActorHandle>{}, flightSound, catchSound, loggedHandAxisDiagnostic = false](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
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

				// Suavizado del tramo final (ver Constants::kReturnTailDistance):
				// el tiempo "de progreso" que alimenta a
				// ComputeTraveledDistance avanza más despacio que el tiempo
				// real cuanto más cerca está la réplica de la mano -- el
				// perfil de aceleración creciente del punto 8 no cambia de
				// forma, solo se recorre más lento en tiempo real en el
				// último tramo. Se mide con la distancia del tick anterior
				// (previousPos), no de nextPos (todavía sin calcular), igual
				// que ya hace Collision::SweepRaycast unas líneas más abajo.
				const float previousDistanceToHand = (handPos - previousPos).Length();
				const float tailBlend = Constants::kReturnTailDistance > 0.0f ? std::clamp(previousDistanceToHand / Constants::kReturnTailDistance, 0.0f, 1.0f) : 1.0f;
				const float smoothTailBlend = tailBlend * tailBlend * (3.0f - 2.0f * tailBlend);
				const float timeRate = Constants::kReturnTailMinRate + (1.0f - Constants::kReturnTailMinRate) * smoothTailBlend;
				progressElapsed += a_deltaSeconds * timeRate;

				const float traveled = ComputeTraveledDistance(acceleration, progressElapsed);
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

				// Misma distancia arma-mano para las dos cosas que dependen
				// de ella: el sonido de atrape se reajusta cada tick con
				// datos reales (ver Audio::CatchSound) en vez de una
				// predicción hecha una sola vez al principio, así que se ata
				// a la misma señal que decide la llegada.
				const float distanceToHand = (handPos - nextPos).Length();
				catchSound->Update(nextPos, distanceToHand, a_deltaSeconds);

				if (distanceToHand <= Constants::kReturnArrivalDistance) {
					logs::info("Return::BeginReturnMovement: la réplica ha llegado a la mano.");
					// Salvaguarda: caso degenerado en el que CatchSound
					// nunca llegó a arrancar (la velocidad de cierre nunca
					// fue positiva) -- mejor un golpe suelto sin
					// sincronizar que ningún sonido.
					if (!catchSound->HasStarted()) {
						logs::warn("Return::BeginReturnMovement: CatchSound nunca llegó a arrancar (velocidad de cierre nunca positiva), disparando sonido suelto sin sincronizar.");
						Audio::PlaySoundOneShot(handPos, Constants::kCatchImpactSoundLocalFormID);
					}
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
