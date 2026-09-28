// Regreso del arma -- ver ReturnManager.h.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "12.- AUDIO/CatchSound.h"
#include "5.- RETURN/ReturnTrajectory.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponTrailGroup.h"
#include "9.- MATH/CurveMath.h"
#include "9.- MATH/RotationMath.h"

#include <algorithm>
#include <numbers>
#include <vector>

namespace Return
{
	namespace
	{
		// Posición del nodo del arma en la mano derecha: destino del regreso.
		RE::NiPoint3 GetHandPosition(RE::Actor* a_player)
		{
			if (auto* handNode = a_player->GetNodeByName("WEAPON")) {
				return handNode->world.translate;
			}

			return a_player->GetPosition();
		}

		// Tiempo real que falta hasta la llegada, simulando paso a paso con las fórmulas del bucle
		// (mano fija). Se corta en a_maxLookahead.
		float SimulateRemainingReturnTime(const RE::NiPoint3& a_currentPos, const RE::NiPoint3& a_handPos, const RE::NiPoint3& a_start, const RE::NiPoint3& a_controlPoint, float a_acceleration, float a_initialDistance, float a_progressElapsed, float a_maxLookahead)
		{
			float        progressElapsed = a_progressElapsed;
			RE::NiPoint3 currentPos = a_currentPos;
			float        remaining = 0.0f;

			while (remaining < a_maxLookahead) {
				const float distanceToHand = (a_handPos - currentPos).Length();
				if (distanceToHand <= Constants::kReturnArrivalDistance) {
					return remaining;
				}

				const float tailBlend = Constants::kReturnTailDistance > 0.0f ? std::clamp(distanceToHand / Constants::kReturnTailDistance, 0.0f, 1.0f) : 1.0f;
				const float smoothTailBlend = tailBlend * tailBlend * (3.0f - 2.0f * tailBlend);
				const float timeRate = Constants::kReturnTailMinRate + (1.0f - Constants::kReturnTailMinRate) * smoothTailBlend;

				progressElapsed += Constants::kTickDeltaSeconds * timeRate;
				remaining += Constants::kTickDeltaSeconds;

				const float traveled = ComputeTraveledDistance(a_acceleration, progressElapsed);
				const float t = a_initialDistance > 0.0f ? std::clamp(traveled / a_initialDistance, 0.0f, 1.0f) : 1.0f;
				currentPos = Math::EvaluateQuadraticBezier(a_start, a_controlPoint, a_handPos, t);

				if (t >= 1.0f) {
					return remaining;
				}
			}

			return a_maxLookahead;
		}

		// Movimiento de vuelta (curva y aceleración); se llama tras el temblor o de inmediato.
		void BeginReturnMovement(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, ReturnCallbacks a_callbacks, std::shared_ptr<Audio::CatchCue> a_catchCue, float a_shudderDuration)
		{
			auto replica = a_replicaHandle.get();
			if (!a_player || !replica) {
				logs::warn("Return::BeginReturnMovement: sin jugador o réplica válida, se aborta el regreso.");
				a_callbacks.onApproaching();
				a_callbacks.onArrived();
				return;
			}

			const auto start = replica->GetPosition();
			const auto initialHandPos = GetHandPosition(a_player);

			const float initialDistance = (initialHandPos - start).Length();
			// Aceleración natural (ComputeReturnAcceleration, acotada por kReturnMaxDuration).
			float acceleration = ComputeReturnAcceleration(initialDistance);

			// Sin temblor, el vuelo se alarga al mínimo que necesita el Atrape
			// (kMinCatchAnimationDelay + kCatchAnimationLeadTime).
			float       estimatedDuration = ComputeReturnDuration(acceleration, initialDistance);
			if (a_shudderDuration <= 0.0f) {
				const float requiredMovementDuration = Constants::kMinCatchAnimationDelay + Constants::kCatchAnimationLeadTime;
				if (estimatedDuration < requiredMovementDuration) {
					acceleration = ComputeReturnAccelerationForDuration(initialDistance, requiredMovementDuration);
					estimatedDuration = requiredMovementDuration;
				}
			}

			const auto controlPoint = ComputeReturnControlPoint(start, initialHandPos, GetPlayerRightVector(a_player), Constants::kReturnCurveAnchorFraction);


			// Sin silbido: el sonido de arranque del atrape ya suena en el regreso.

			// Rotación del nodo de giro al empezar el tramo (base del giro) y del nodo raíz (constante).
			const RE::NiMatrix3 rootWorld = replica->Get3D() ? replica->Get3D()->world.rotate : RE::NiMatrix3{};
			const RE::NiMatrix3 movementBaseLocal = Animation::GetSpinLocalRotation(*replica);

			// Estela del tramo de regreso.
			auto trail = std::make_shared<Animation::WeaponTrailGroup>();

			// Plano de la estela: normal del plano de la Bezier, con el signo del eje Z del arma.
			RE::NiPoint3 trailUpReference = (controlPoint - start).Cross(initialHandPos - start);
			const float  trailUpLength = trailUpReference.Length();
			trailUpReference = trailUpLength > 0.0f ? trailUpReference / trailUpLength : RE::NiPoint3{ 0.0f, 0.0f, 1.0f };

			// Roll fijo Constants::kTrailRollDegrees.
			const float trailRoll = Constants::kTrailRollDegrees * std::numbers::pi_v<float> / 180.0f;

			// Offset de anclaje rotado con rootWorld.
			const RE::NiPoint3 trailAnchorWorldOffset = rootWorld * Constants::kTrailAnchorLocalOffset;

			trail->Start(replica->GetParentCell(), start, trailUpReference, trailRoll, trailAnchorWorldOffset);

			auto token = Physics::StartTickLoop(a_replicaHandle, [a_player, start, controlPoint, initialDistance, acceleration, rootWorld, movementBaseLocal, trail, trailUpReference, trailRoll, onArrived = a_callbacks.onArrived, onApproaching = a_callbacks.onApproaching, shudderDuration = a_shudderDuration, catchTriggered = false, straightening = false, arrivedFired = false, straightenStart = 0.0f, straightenDuration = Constants::kSpinStraightenLeadTime, straightenBlendFromLocal = RE::NiMatrix3{}, elapsed = 0.0f, progressElapsed = 0.0f, hitActors = std::vector<RE::ActorHandle>{}, catchCue = std::move(a_catchCue)](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				const auto previousPos = a_refr.GetPosition();
				elapsed += a_deltaSeconds;

				const auto handPos = GetHandPosition(a_player);

				// Tras la llegada, la réplica se pega a la mano cada tick hasta que ReequipAndReset cancele el bucle.
				if (arrivedFired) {
					a_refr.SetPosition(handPos);
					Physics::SyncHavok(a_refr, handPos, a_refr.GetAngle());
					return true;
				}

				// Giro por código; en la ventana final se sustituye por el enderezado hacia la rotación
				// actual del hueso de la mano.
				if (straightening) {
					const float         blend = (elapsed - straightenStart) / straightenDuration;
					const RE::NiMatrix3 handTargetLocal = Math::LocalRotationFromWorld(rootWorld, Animation::GetHandBoneWorldRotation(*a_player));
					Animation::TickSpinStraighten(a_refr, straightenBlendFromLocal, handTargetLocal, blend);
				} else {
					Animation::TickSpin(a_refr, elapsed, movementBaseLocal);
				}

				// Tramo final más lento: el tiempo de progreso avanza menos cerca de la mano (kReturnTailDistance).
				const float previousDistanceToHand = (handPos - previousPos).Length();
				const float tailBlend = Constants::kReturnTailDistance > 0.0f ? std::clamp(previousDistanceToHand / Constants::kReturnTailDistance, 0.0f, 1.0f) : 1.0f;
				const float smoothTailBlend = tailBlend * tailBlend * (3.0f - 2.0f * tailBlend);
				const float timeRate = Constants::kReturnTailMinRate + (1.0f - Constants::kReturnTailMinRate) * smoothTailBlend;
				progressElapsed += a_deltaSeconds * timeRate;

				const float traveled = ComputeTraveledDistance(acceleration, progressElapsed);
				const float t = initialDistance > 0.0f ? std::clamp(traveled / initialDistance, 0.0f, 1.0f) : 1.0f;

				const auto nextPos = Math::EvaluateQuadraticBezier(start, controlPoint, handPos, t);

				// Golpe a cada actor que atraviesa, una sola vez por regreso; el escenario se ignora.
				const auto hit = Collision::SweepRaycast(previousPos, nextPos, Constants::kThrowCollisionRadius, a_player, &a_refr);
				// Los cadáveres se ignoran.
				auto* actor = hit.hit && hit.target ? hit.target->As<RE::Actor>() : nullptr;
				if (actor && !actor->IsDead()) {
					RE::ActorHandle actorHandle(actor);
					if (std::ranges::find(hitActors, actorHandle) == hitActors.end()) {
						hitActors.push_back(actorHandle);
						Combat::ApplyReturnHit(a_player, actor, hit.point);
					}
				}

				a_refr.SetPosition(nextPos);
				Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());

				// Avanza el reloj del sonido de arranque del atrape.
				catchCue->UpdateStart(nextPos, a_deltaSeconds);

				// Durante el enderezado, el roll de la estela se funde hacia la orientación de la mano.
				if (straightening) {
					const RE::NiPoint3 travelDir = nextPos - previousPos;
					const float        travelLength = travelDir.Length();
					if (travelLength > 0.0f) {
						// Mismo signo que el usado para el eje Z del arma.
						const RE::NiPoint3 handUpAxis = -Animation::GetHandBoneWorldRotation(*a_player).GetVectorZ();
						const float        targetRoll = Math::ComputeRoll(travelDir / travelLength, trailUpReference, handUpAxis);

						float           diff = targetRoll - trailRoll;
						constexpr float pi = std::numbers::pi_v<float>;
						while (diff > pi) {
							diff -= 2.0f * pi;
						}
						while (diff < -pi) {
							diff += 2.0f * pi;
						}

						const float straightenBlend = std::clamp((elapsed - straightenStart) / straightenDuration, 0.0f, 1.0f);
						trail->SetRoll(trailRoll + diff * straightenBlend);
					}
				}

				// Tras la llegada la estela deja de alimentarse y queda congelada.
				trail->Update(nextPos, a_deltaSeconds);

				const float distanceToHand = (handPos - nextPos).Length();

				// Con el tiempo restante simulado: aviso onApproaching (kCatchAnimationLeadTime)
				// y arranque del enderezado (kSpinStraightenLeadTime), por separado.
				if (!catchTriggered || !straightening) {
					// Tope de la simulación por encima del mayor umbral consultado.
					constexpr float kLookaheadCap = Constants::kCatchAnimationLeadTime + Constants::kCatchApproachSafetyMargin + 0.1f;
					const float     estimatedTimeToArrival = SimulateRemainingReturnTime(nextPos, handPos, start, controlPoint, acceleration, initialDistance, progressElapsed, kLookaheadCap);

					if (!catchTriggered) {
						const bool settledSinceCall = shudderDuration + elapsed >= Constants::kMinCatchAnimationDelay;
						// + kCatchApproachSafetyMargin para que la llegada física gane a la anotación de Catch.hkx.
						if (settledSinceCall && estimatedTimeToArrival <= Constants::kCatchAnimationLeadTime + Constants::kCatchApproachSafetyMargin) {
							catchTriggered = true;
							onApproaching();
						}
					}

					if (!straightening && estimatedTimeToArrival <= Constants::kSpinStraightenLeadTime) {
						straightening = true;
						straightenStart = elapsed;
						// La ventana de enderezado dura lo que falte hasta la llegada, con un mínimo.
						constexpr float kMinStraightenBlendDuration = 0.05f;
						straightenDuration = estimatedTimeToArrival > kMinStraightenBlendDuration ? estimatedTimeToArrival : kMinStraightenBlendDuration;
						straightenBlendFromLocal = Animation::GetSpinLocalRotation(a_refr);
					}
				}

				if (distanceToHand <= Constants::kReturnArrivalDistance) {
					// Redes de seguridad: onApproaching y el enderezado se disparan antes de onArrived si no lo hicieron.
					if (!catchTriggered) {
						catchTriggered = true;
						onApproaching();
					}
					if (!straightening) {
						straightening = true;

						// Llegada sin ventana de enderezado: se endereza del todo en este tick.
						const RE::NiMatrix3 handTargetLocal = Math::LocalRotationFromWorld(rootWorld, Animation::GetHandBoneWorldRotation(*a_player));
						Animation::TickSpinStraighten(a_refr, Animation::GetSpinLocalRotation(a_refr), handTargetLocal, 1.0f);
					}
					onArrived();
					// El bucle sigue vivo pegando la réplica a la mano.
					arrivedFired = true;
					return true;
				}

				return true;
			});

			a_callbacks.onTickStarted(token);
		}
	}

	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, bool a_wasStuck, ReturnCallbacks a_callbacks)
	{
		auto replica = a_replicaHandle.get();
		if (!a_player || !replica) {
			logs::warn("Return::BeginReturn: sin jugador o réplica válida, se aborta el regreso.");
			a_callbacks.onApproaching();
			a_callbacks.onArrived();
			return;
		}

		// Estimación del tramo de movimiento a velocidad natural (para el sonido y el temblor).
		const float initialDistanceForCue = (GetHandPosition(a_player) - replica->GetPosition()).Length();
		const float accelerationForCue = ComputeReturnAcceleration(initialDistanceForCue);
		const float predictedMovementDuration = ComputeReturnDuration(accelerationForCue, initialDistanceForCue);

		// Temblor alargado si el vuelo no da el tiempo mínimo que necesita el Atrape (solo si estaba clavada).
		const float requiredTotalForSettle = Constants::kMinCatchAnimationDelay + Constants::kCatchAnimationLeadTime;
		const float shudderDeficit = requiredTotalForSettle - predictedMovementDuration;
		const float shudderDuration = a_wasStuck ?
		                                  (shudderDeficit > Constants::kStickShudderDuration ? shudderDeficit : Constants::kStickShudderDuration) :
		                                  0.0f;

		// Retardo del sonido de arranque del atrape, contando temblor y vuelo (ternario: max es macro).
		const float rawStartDelay = shudderDuration + predictedMovementDuration - Constants::kCatchStartSoundLeadTime;
		const float startDelay = rawStartDelay > 0.0f ? rawStartDelay : 0.0f;
		auto        catchCue = std::make_shared<Audio::CatchCue>(startDelay);

		if (!a_wasStuck) {
			BeginReturnMovement(a_player, a_replicaHandle, std::move(a_callbacks), catchCue, shudderDuration);
			return;
		}

		// Temblor sin mover la réplica sobre su rotación al clavarse; al acabar arranca BeginReturnMovement.
		RE::NiMatrix3 baseRotation;
		if (auto* root = replica->Get3D()) {
			if (auto* spinNode = root->GetObjectByName(Constants::kWeaponSpinNodeName)) {
				baseRotation = spinNode->local.rotate;
			}
		}


		auto shudderToken = Physics::StartTickLoop(a_replicaHandle, [a_player, a_replicaHandle, callbacks = a_callbacks, baseRotation, catchCue, shudderDuration, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			elapsed += a_deltaSeconds;

			// El reloj del sonido sigue contando durante el temblor.
			catchCue->UpdateStart(a_refr.GetPosition(), a_deltaSeconds);

			if (elapsed >= shudderDuration) {
				BeginReturnMovement(a_player, a_replicaHandle, std::move(callbacks), std::move(catchCue), shudderDuration);
				return false;
			}

			// Update3DPosition para que el giro local llegue a world.rotate (la réplica no se mueve).
			Animation::TickShudder(a_refr, baseRotation, elapsed, shudderDuration);
			a_refr.Update3DPosition(true);
			return true;
		});

		a_callbacks.onTickStarted(shudderToken);
	}
}
