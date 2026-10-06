// Regreso del arma -- ver ReturnManager.h.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/FrameHook.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "12.- AUDIO/CatchSound.h"
#include "5.- RETURN/ReturnTrajectory.h"
#include "6.- PHYSICS/CollisionManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/WeaponAnimation.h"
#include "8.- ANIMATION/WeaponTrailGroup.h"
#include "9.- MATH/CurveMath.h"
#include "9.- MATH/RotationMath.h"
#include "9.- MATH/VectorMath.h"

#include <algorithm>
#include <chrono>
#include <numbers>
#include <vector>

namespace Return
{
	void CatchSync::OnCatchStarted()
	{
		deadline = FrameHook::Now() + leadSeconds;
	}

	void CatchSync::MarkRequested()
	{
		if (!requestedAt) {
			requestedAt = FrameHook::Now();
		}
	}

	std::optional<float> CatchSync::GetSecondsToDeadline() const
	{
		if (!deadline) {
			return std::nullopt;
		}

		return static_cast<float>(*deadline - FrameHook::Now());
	}

	bool CatchSync::IsFree()
	{
		if (released) {
			return true;
		}

		const double startTimeout = std::chrono::duration<double>(Constants::kCatchStartTimeout).count();
		if (requestedAt && !deadline && FrameHook::Now() - *requestedAt > startTimeout) {
			logs::warn("Return::CatchSync: Catch.hkx no empezó tras pedir el Atrape; el arma vuelve sin esperarlo.");
			released = true;
			return true;
		}

		return false;
	}

	namespace
	{
		// Segundos de FrameHook::Now (el reloj de Catch.hkx) desde a_from.
		float SecondsSince(double a_from)
		{
			return static_cast<float>(FrameHook::Now() - a_from);
		}

		// Tiempo que falta hasta la llegada a ritmo natural, simulando paso a paso con las fórmulas
		// del bucle (mano fija). Se corta en a_maxLookahead.
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

				progressElapsed += Constants::kTickDeltaSeconds * ComputeTailTimeRate(distanceToHand);
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

		// Duración natural de un vuelo de a_from a a_handPos (en línea recta, con el tramo final lento).
		// La usa el temblor para decidir cuándo pedir el Atrape y cuándo despegar.
		float EstimateFlightDuration(const RE::NiPoint3& a_from, const RE::NiPoint3& a_handPos)
		{
			const float distance = (a_handPos - a_from).Length();
			const auto  midpoint = (a_from + a_handPos) * 0.5f;
			return SimulateRemainingReturnTime(a_from, a_handPos, a_from, midpoint, ComputeReturnAcceleration(distance), distance, 0.0f, Constants::kReturnArrivalLookahead);
		}

		// Movimiento de vuelta (curva y aceleración); se llama tras el temblor o de inmediato.
		// a_callTime: instante (FrameHook::Now) en que se soltó la Llamada.
		void BeginReturnMovement(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, ReturnCallbacks a_callbacks, std::shared_ptr<Audio::CatchCue> a_catchCue, std::shared_ptr<CatchSync> a_catchSync, double a_callTime, bool a_wasStuck)
		{
			auto replica = a_replicaHandle.get();
			if (!a_player || !replica) {
				logs::warn("Return::BeginReturnMovement: sin jugador o réplica válida, se aborta el regreso.");
				a_callbacks.onApproaching();
				a_callbacks.onArrived();
				return;
			}

			const auto start = replica->GetPosition();
			const auto initialHandPos = ActorUtils::GetWeaponBonePosition(*a_player);

			const float initialDistance = (initialHandPos - start).Length();
			// Aceleración natural (ComputeReturnAcceleration, acotada por kReturnMaxDuration).
			float acceleration = ComputeReturnAcceleration(initialDistance);

			// Sin temblor que absorba la espera, el vuelo se alarga al mínimo que necesita el Atrape
			// (kMinCatchAnimationDelay + duración de Catch.hkx hasta su anotación).
			if (!a_wasStuck) {
				const float requiredMovementDuration = Constants::kMinCatchAnimationDelay + a_catchSync->GetLeadSeconds();
				if (ComputeReturnDuration(acceleration, initialDistance) < requiredMovementDuration) {
					acceleration = ComputeReturnAccelerationForDuration(initialDistance, requiredMovementDuration);
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
			const auto trailUpReference = Math::NormalizedOr((controlPoint - start).Cross(initialHandPos - start), Math::kWorldUp);

			// Roll fijo Constants::kTrailRollDegrees.
			const float trailRoll = Math::DegreesToRadians(Constants::kTrailRollDegrees);

			// Offset de anclaje rotado con rootWorld.
			const RE::NiPoint3 trailAnchorWorldOffset = rootWorld * Constants::kTrailAnchorLocalOffset;

			trail->Start(replica->GetParentCell(), start, trailUpReference, trailRoll, trailAnchorWorldOffset);

			auto token = Physics::StartTickLoop(a_replicaHandle, [a_player, start, controlPoint, initialDistance, acceleration, rootWorld, movementBaseLocal, trail, trailUpReference, trailRoll, onArrived = a_callbacks.onArrived, onApproaching = a_callbacks.onApproaching, straightening = false, arrivedFired = false, straightenStart = 0.0f, straightenDuration = Constants::kSpinStraightenLeadTime, straightenBlendFromLocal = RE::NiMatrix3{}, elapsed = 0.0f, progressElapsed = 0.0f, hitActors = std::vector<RE::ActorHandle>{}, catchCue = std::move(a_catchCue), catchSync = std::move(a_catchSync), callTime = a_callTime](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
				const float deltaSeconds = a_deltaSeconds;
				const auto  previousPos = a_refr.GetPosition();
				elapsed += deltaSeconds;

				const auto handPos = ActorUtils::GetWeaponBonePosition(*a_player);

				// Tras la llegada, la réplica se pega a la mano cada tick hasta que ReequipAndReset cancele el bucle.
				if (arrivedFired) {
					Physics::MoveTo(a_refr, handPos);
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

				// Con Catch.hkx en marcha, el ritmo se reescala para llegar a su anotación
				// (acotado entre kReturnRetimeMinRate y kReturnRetimeMaxRate).
				const float naturalRemaining = SimulateRemainingReturnTime(previousPos, handPos, start, controlPoint, acceleration, initialDistance, progressElapsed, Constants::kReturnArrivalLookahead);
				float       retimeRate = 1.0f;
				if (const auto toDeadline = catchSync->GetSecondsToDeadline()) {
					retimeRate = *toDeadline > 0.0f ? std::clamp(naturalRemaining / *toDeadline, Constants::kReturnRetimeMinRate, Constants::kReturnRetimeMaxRate) : Constants::kReturnRetimeMaxRate;
				}
				const float timeToArrival = naturalRemaining / retimeRate;

				// Tramo final más lento: el tiempo de progreso avanza menos cerca de la mano (kReturnTailDistance).
				progressElapsed += deltaSeconds * ComputeTailTimeRate((handPos - previousPos).Length()) * retimeRate;

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

				Physics::MoveTo(a_refr, nextPos);

				// Sonido de arranque del atrape con la llegada prevista.
				catchCue->UpdateStart(nextPos, timeToArrival);

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
				trail->Update(nextPos, deltaSeconds);

				const float distanceToHand = (handPos - nextPos).Length();

				// Pide el Atrape cuando la llegada queda a la duración de Catch.hkx hasta su anotación,
				// nunca antes de kMinCatchAnimationDelay desde la Llamada.
				const float sinceCall = SecondsSince(callTime);
				if (!catchSync->IsRequested() && sinceCall >= Constants::kMinCatchAnimationDelay && timeToArrival <= catchSync->GetLeadSeconds()) {
					catchSync->MarkRequested();
					onApproaching();
				}

				// Enderezado en la ventana final (kSpinStraightenLeadTime).
				if (!straightening && timeToArrival <= Constants::kSpinStraightenLeadTime) {
					straightening = true;
					straightenStart = elapsed;
					// La ventana de enderezado dura lo que falte hasta la llegada, con un mínimo.
					constexpr float kMinStraightenBlendDuration = 0.05f;
					straightenDuration = timeToArrival > kMinStraightenBlendDuration ? timeToArrival : kMinStraightenBlendDuration;
					straightenBlendFromLocal = Animation::GetSpinLocalRotation(a_refr);
				}

				if (distanceToHand <= Constants::kReturnArrivalDistance) {
					// Redes de seguridad: onApproaching y el enderezado se disparan antes de onArrived si no lo hicieron.
					if (!catchSync->IsRequested()) {
						catchSync->MarkRequested();
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

	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, bool a_wasStuck, std::shared_ptr<CatchSync> a_catchSync, ReturnCallbacks a_callbacks)
	{
		auto replica = a_replicaHandle.get();
		if (!a_player || !replica || !a_catchSync) {
			logs::warn("Return::BeginReturn: sin jugador, réplica o sincronía válida, se aborta el regreso.");
			a_callbacks.onApproaching();
			a_callbacks.onArrived();
			return;
		}

		const double callTime = FrameHook::Now();
		auto         catchCue = std::make_shared<Audio::CatchCue>();

		if (!a_wasStuck) {
			BeginReturnMovement(a_player, a_replicaHandle, std::move(a_callbacks), std::move(catchCue), std::move(a_catchSync), callTime, false);
			return;
		}

		// Duración prevista del temblor, solo para su rampa visual (TickShudder): espera mínima del Atrape
		// menos el vuelo natural, con un mínimo de kStickShudderDuration. El final real lo decide el bucle.
		const float initialFlight = EstimateFlightDuration(replica->GetPosition(), ActorUtils::GetWeaponBonePosition(*a_player));
		const float shudderForCatch = Constants::kMinCatchAnimationDelay + a_catchSync->GetLeadSeconds() - initialFlight;
		const float plannedShudder = shudderForCatch > Constants::kStickShudderDuration ? shudderForCatch : Constants::kStickShudderDuration;

		// Temblor sin mover la réplica sobre su rotación al clavarse; al despegar arranca BeginReturnMovement.
		const RE::NiMatrix3 baseRotation = Animation::GetSpinLocalRotation(*replica);

		auto shudderToken = Physics::StartTickLoop(a_replicaHandle, [a_player, a_replicaHandle, callbacks = a_callbacks, baseRotation, catchCue, catchSync = a_catchSync, callTime, plannedShudder, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			elapsed += a_deltaSeconds;

			const auto  replicaPos = a_refr.GetPosition();
			const float flight = EstimateFlightDuration(replicaPos, ActorUtils::GetWeaponBonePosition(*a_player));
			const float lead = catchSync->GetLeadSeconds();
			const auto  toDeadline = catchSync->GetSecondsToDeadline();

			// Llegada prevista: la fijada por Catch.hkx si ya empezó; si no, la más temprana posible
			// (temblor mínimo + vuelo, y nunca antes de que Catch.hkx pueda cerrar la mano).
			const float earliestByFlight = Constants::kStickShudderDuration + flight;
			const float earliestByCatch = Constants::kMinCatchAnimationDelay + lead;
			const float timeToArrival = toDeadline ? *toDeadline : (earliestByFlight > earliestByCatch ? earliestByFlight : earliestByCatch) - elapsed;

			// El reloj del sonido sigue contando durante el temblor.
			catchCue->UpdateStart(replicaPos, timeToArrival);

			// Pide el Atrape cuando, despegando cuanto antes, el arma llegaría dentro de la duración de Catch.hkx.
			const float minShudderLeft = Constants::kStickShudderDuration > elapsed ? Constants::kStickShudderDuration - elapsed : 0.0f;
			if (!catchSync->IsRequested() && elapsed >= Constants::kMinCatchAnimationDelay && minShudderLeft + flight <= lead) {
				catchSync->MarkRequested();
				callbacks.onApproaching();
			}

			// Despegue, pasado el temblor mínimo: con Catch.hkx en marcha, cuando el vuelo natural llega justo
			// a su anotación; si el vuelo es más largo que Catch.hkx o ya no hay que esperarlo, de inmediato.
			bool depart = false;
			if (elapsed >= Constants::kStickShudderDuration) {
				if (toDeadline) {
					depart = *toDeadline <= flight;
				} else {
					depart = catchSync->IsFree() || (!catchSync->IsRequested() && flight > lead);
				}
			}

			if (depart) {
				BeginReturnMovement(a_player, a_replicaHandle, std::move(callbacks), std::move(catchCue), std::move(catchSync), callTime, true);
				return false;
			}

			// Update3DPosition para que el giro local llegue a world.rotate (la réplica no se mueve).
			Animation::TickShudder(a_refr, baseRotation, elapsed, plannedShudder);
			a_refr.Update3DPosition(true);
			return true;
		});

		a_callbacks.onTickStarted(shudderToken);
	}
}
