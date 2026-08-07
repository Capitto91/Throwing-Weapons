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
			return 0.0f;
		}

		// Aceleración que hace que la velocidad de llegada (v(T), con
		// T = duración total) sea exactamente Constants::kReturnTargetArrivalSpeed
		// sin importar a_distance -- despeje simultáneo de d(T)=a_distance
		// (T = n·a_distance/vf, sustituyendo en v(T)=a/(n-1)·T^(n-1)) y
		// v(T)=vf: a = (n-1)/n^(n-1) · vf^n / a_distance^(n-1). A diferencia
		// de un coeficiente fijo, esto evita que un regreso corto se quede
		// todo el trayecto dentro del primer tramo de la rampa sin llegar a
		// coger velocidad (ver CLAUDE.md, 2026-08-07).
		constexpr float vf = Constants::kReturnTargetArrivalSpeed;
		const float     defaultAcceleration = (n - 1.0f) / std::pow(n, n - 1.0f) * std::pow(vf, n) / std::pow(a_distance, n - 1.0f);

		// T = n·a_distance/vf -- mismo despeje que arriba, para T en vez de
		// para a.
		const float defaultDuration = n * a_distance / vf;
		if (defaultDuration <= Constants::kReturnMaxDuration) {
			return defaultAcceleration;
		}

		// A partir de aquí se sacrifica la velocidad de llegada constante:
		// mismo despeje que ComputeTraveledDistance pero al revés (a partir
		// de T fijo en kReturnMaxDuration): a = d·n·(n-1)/T^n.
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

		// Despeje directo de ComputeTraveledDistance (d = a/(n·(n-1))·T^n),
		// mismo criterio de forma cerrada que el resto del módulo.
		return std::pow(a_distance * n * (n - 1.0f) / a_acceleration, 1.0f / n);
	}

	float ComputeReturnAccelerationForDuration(float a_distance, float a_targetDuration)
	{
		constexpr float n = Constants::kReturnAccelerationExponent;

		// Mismo despeje que el límite superior dentro de
		// ComputeReturnAcceleration (a = d·n·(n-1)/T^n), aquí parametrizado
		// por a_targetDuration en vez de la constante fija
		// kReturnMaxDuration.
		return a_distance * n * (n - 1.0f) / std::pow(a_targetDuration, n);
	}
}
