// Implementación del sistema de retorno.
// Controla velocidad, duración máxima del viaje y sincronización con la mano.

#include "5.- RETURN/ReturnManager.h"

#include "1.- CORE/Constants.h"
#include "5.- RETURN/ReturnTrajectory.h"
#include "9.- MATH/CurveMath.h"

#include <algorithm>

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
	}

	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, ReturnCallbacks a_callbacks)
	{
		auto replica = a_replicaHandle.get();
		if (!a_player || !replica) {
			logs::warn("Return::BeginReturn: sin jugador o réplica válida, se aborta el regreso.");
			a_callbacks.onArrived();
			return;
		}

		const auto start = replica->GetPosition();
		const auto initialHandPos = GetHandPosition(a_player);

		const float initialDistance = (initialHandPos - start).Length();
		const float acceleration = ComputeReturnAcceleration(initialDistance);
		const auto  controlPoint = ComputeReturnControlPoint(start, initialHandPos, GetPlayerRightVector(a_player));

		logs::info(
			"Return::BeginReturn: distancia inicial {:.1f}, aceleración {:.1f}",
			initialDistance, acceleration);

		auto token = Physics::StartTickLoop(a_replicaHandle, [a_player, start, controlPoint, initialDistance, acceleration, onArrived = a_callbacks.onArrived, elapsed = 0.0f](RE::TESObjectREFR& a_refr, float a_deltaSeconds) mutable {
			elapsed += a_deltaSeconds;

			// El extremo final de la curva se recalcula cada tick (a
			// diferencia del inicio y el punto de control, fijados una
			// única vez): así el regreso no pierde de vista al jugador si
			// se mueve mientras el arma vuela de vuelta.
			const auto handPos = GetHandPosition(a_player);

			const float traveled = ComputeTraveledDistance(acceleration, elapsed);
			const float t = initialDistance > 0.0f ? std::clamp(traveled / initialDistance, 0.0f, 1.0f) : 1.0f;

			const auto nextPos = Math::EvaluateQuadraticBezier(start, controlPoint, handPos, t);
			a_refr.SetPosition(nextPos);
			Physics::SyncHavok(a_refr, nextPos, a_refr.GetAngle());

			if ((handPos - nextPos).Length() <= Constants::kReturnArrivalDistance) {
				logs::info("Return::BeginReturn: la réplica ha llegado a la mano.");
				onArrived();
				return false;
			}

			return true;
		});

		a_callbacks.onTickStarted(token);
	}
}
