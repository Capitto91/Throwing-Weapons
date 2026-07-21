// Implementación de las trayectorias curvas de retorno.
// Calcula posiciones intermedias y adapta el camino según enemigos cercanos.

#include "5.- RETURN/ReturnTrajectory.h"

#include "1.- CORE/Constants.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace Return
{
	namespace
	{
		// Motor de aleatoriedad propio de este módulo, reutilizado entre
		// llamadas (no tiene sentido reconstruirlo cada vez) — solo se
		// invoca desde el hilo principal (BeginReturn), así que no hace
		// falta protegerlo entre hilos.
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

		// Gram-Schmidt: proyecta a_rightVector perpendicular a la línea
		// recta para decidir el lado. Caso degenerado (vector "derecha"
		// casi paralelo a la línea recta): se cae a perpendicular sobre
		// el eje Z del mundo, igual que Collision::SweepRaycast resuelve
		// el mismo problema para su base perpendicular.
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
			return Constants::kReturnAcceleration;
		}

		// d(T) = a/(n·(n-1))·T^n despejando T, para el coeficiente por
		// defecto -- generaliza el sqrt(2d/a) anterior (caso n=2).
		const float defaultDuration = std::pow(a_distance * n * (n - 1.0f) / Constants::kReturnAcceleration, 1.0f / n);
		if (defaultDuration <= Constants::kReturnMaxDuration) {
			return Constants::kReturnAcceleration;
		}

		// Mismo despeje que arriba pero al revés (a partir de T fijo en
		// kReturnMaxDuration): a = d·n·(n-1)/T^n.
		return a_distance * n * (n - 1.0f) / std::pow(Constants::kReturnMaxDuration, n);
	}

	float ComputeTraveledDistance(float a_acceleration, float a_elapsedSeconds)
	{
		constexpr float n = Constants::kReturnAccelerationExponent;
		return a_acceleration / (n * (n - 1.0f)) * std::pow(a_elapsedSeconds, n);
	}
}
