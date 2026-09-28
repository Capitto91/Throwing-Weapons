// Cálculos del regreso -- ver ReturnTrajectory.h.

#include "5.- RETURN/ReturnTrajectory.h"

#include "1.- CORE/Constants.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace Return
{
	namespace
	{
		// Generador aleatorio del módulo (solo hilo principal).
		float RandomLateralFraction()
		{
			static std::mt19937                  rng{ std::random_device{}() };
			std::uniform_real_distribution<float> dist(Constants::kReturnCurveLateralFractionMin, Constants::kReturnCurveLateralFractionMax);
			return dist(rng);
		}
	}

	RE::NiPoint3 GetPlayerRightVector(RE::Actor* a_actor)
	{
		if (auto* node = a_actor ? a_actor->Get3D() : nullptr) {
			return node->world.rotate.GetVectorX();
		}

		return { 1.0f, 0.0f, 0.0f };
	}

	RE::NiPoint3 ComputeReturnControlPoint(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, const RE::NiPoint3& a_rightVector, float a_anchorFraction)
	{
		const auto  straight = a_end - a_start;
		const float distance = straight.Length();
		if (distance <= 0.0f) {
			return a_start;
		}

		const auto forward = straight / distance;

		// Proyecta a_rightVector perpendicular a la línea; si es casi paralelo usa el eje Z.
		auto  side = a_rightVector - forward * a_rightVector.Dot(forward);
		float sideLength = side.Length();
		if (sideLength < 0.01f) {
			side = forward.Cross(RE::NiPoint3{ 0.0f, 0.0f, 1.0f });
			sideLength = side.Length();
		}
		if (sideLength < 0.01f) {
			side = { 1.0f, 0.0f, 0.0f };
			sideLength = 1.0f;
		}
		side = side / sideLength;

		const float offset = std::clamp(distance * RandomLateralFraction(), Constants::kReturnCurveMinOffset, Constants::kReturnCurveMaxOffset);
		const auto  anchorPoint = a_start + straight * a_anchorFraction;

		return anchorPoint + side * offset;
	}

	float ComputeReturnAcceleration(float a_distance)
	{
		constexpr float n = Constants::kReturnAccelerationExponent;

		if (a_distance <= 0.0f) {
			return 0.0f;
		}

		// Aceleración que da la velocidad de llegada objetivo: a = (n-1)/n^(n-1) · vf^n / d^(n-1).
		constexpr float vf = Constants::kReturnTargetArrivalSpeed;
		const float     defaultAcceleration = (n - 1.0f) / std::pow(n, n - 1.0f) * std::pow(vf, n) / std::pow(a_distance, n - 1.0f);

		// T = n·d/vf.
		const float defaultDuration = n * a_distance / vf;
		if (defaultDuration <= Constants::kReturnMaxDuration) {
			return defaultAcceleration;
		}

		// Si supera la duración máxima: a = d·n·(n-1)/T^n con T = kReturnMaxDuration.
		return a_distance * n * (n - 1.0f) / std::pow(Constants::kReturnMaxDuration, n);
	}

	float ComputeTraveledDistance(float a_acceleration, float a_elapsedSeconds)
	{
		constexpr float n = Constants::kReturnAccelerationExponent;
		return a_acceleration / (n * (n - 1.0f)) * std::pow(a_elapsedSeconds, n);
	}

	float ComputeReturnDuration(float a_acceleration, float a_distance)
	{
		constexpr float n = Constants::kReturnAccelerationExponent;

		if (a_distance <= 0.0f || a_acceleration <= 0.0f) {
			return 0.0f;
		}

		// T = (d·n·(n-1)/a)^(1/n).
		return std::pow(a_distance * n * (n - 1.0f) / a_acceleration, 1.0f / n);
	}

	float ComputeReturnAccelerationForDuration(float a_distance, float a_targetDuration)
	{
		constexpr float n = Constants::kReturnAccelerationExponent;

		// a = d·n·(n-1)/T^n con T = a_targetDuration.
		return a_distance * n * (n - 1.0f) / std::pow(a_targetDuration, n);
	}
}
