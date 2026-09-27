// Implementación del puente opcional hacia True Directional Movement.
//
// Atribución:
//   Parte de este archivo es código copiado de True Directional Movement,
//   de ersh1 -- https://github.com/ersh1/TrueDirectionalMovement
//   Licencia GPL-3.0 (misma licencia que este proyecto).
//   Copiado de ese repositorio:
//   - PredictAimProjectile y ApproximatelyEqual (src/Utils.cpp, src/Utils.h),
//     sin cambios de lógica. La función original de TDM está adaptada a su
//     vez de http://ringofblades.com/Blades/Code/PredictiveAim.cs (fuente
//     citada en el propio código de TDM).
//   - La elección del punto del cuerpo al que apuntar, reproducida en
//     GetTargetPoint a partir de DirectionalMovementHandler::GetTargetPoints
//     y DirectionalMovementHandler::GetTargetPosition
//     (src/DirectionalMovementHandler.cpp).
//   El header de su API (src/13.- EXTERNAL/TrueDirectionalMovement/
//   TrueDirectionalMovementAPI.h) es una copia literal, sin modificar.

#include "11.- SKYRIM/TDMBridge.h"

#include "13.- EXTERNAL/TrueDirectionalMovement/TrueDirectionalMovementAPI.h"

#include <cfloat>
#include <cmath>

namespace TDMBridge
{
	namespace
	{
		// V1 basta: solo se usan GetTargetLockState/GetCurrentTarget, que ya
		// existen desde la primera versión de la interfaz -- pedir la
		// mínima deja funcionar el puente con cualquier TDM que tenga API.
		TDM_API::IVTDM1* g_api = nullptr;

		// Mismo helper que TDM (src/Utils.h).
		bool ApproximatelyEqual(float a_lhs, float a_rhs)
		{
			return ((a_lhs - a_rhs) < FLT_EPSILON) && ((a_rhs - a_lhs) < FLT_EPSILON);
		}
	}

	void Init()
	{
		g_api = static_cast<TDM_API::IVTDM1*>(TDM_API::RequestPluginAPI(TDM_API::InterfaceVersion::V1));
		if (g_api) {
			logs::info("TDMBridge::Init: API de True Directional Movement obtenida, apuntado con target lock activo.");
		} else {
			logs::info("TDMBridge::Init: True Directional Movement no está instalado (o no tiene API), Lanzar se comporta sin target lock.");
		}
	}

	RE::NiPointer<RE::Actor> GetLockedTarget()
	{
		if (!g_api || !g_api->GetTargetLockState()) {
			return nullptr;
		}

		auto target = g_api->GetCurrentTarget().get();
		if (!target || target->IsDead()) {
			return nullptr;
		}
		return target;
	}

	RE::NiPoint3 GetTargetPoint(RE::Actor& a_target)
	{
		auto* race = a_target.GetRace();
		auto* bodyPartData = race ? race->bodyPartData : nullptr;
		auto* torso = bodyPartData ? bodyPartData->parts[RE::BGSBodyPartDefs::LIMB_ENUM::kTorso] : nullptr;
		auto* actor3D = a_target.Get3D2();

		if (torso && actor3D && !torso->targetName.empty()) {
			if (auto* node = actor3D->GetObjectByName(torso->targetName)) {
				return node->world.translate;
			}
		}

		return a_target.GetLookingAtLocation();
	}

	bool PredictAimProjectile(const RE::NiPoint3& a_projectilePos, const RE::NiPoint3& a_targetPosition, const RE::NiPoint3& a_targetVelocity, float a_gravity, RE::NiPoint3& a_projectileVelocity)
	{
		// Copia literal de TDM (ver el header), sin cambios de lógica.
		const float projectileSpeedSquared = a_projectileVelocity.SqrLength();
		const float projectileSpeed = std::sqrtf(projectileSpeedSquared);

		if (projectileSpeed <= 0.f || a_projectilePos == a_targetPosition) {
			return false;
		}

		const float        targetSpeedSquared = a_targetVelocity.SqrLength();
		const float        targetSpeed = std::sqrtf(targetSpeedSquared);
		const RE::NiPoint3 targetToProjectile = a_projectilePos - a_targetPosition;
		const float        distanceSquared = targetToProjectile.SqrLength();
		const float        distance = std::sqrtf(distanceSquared);
		RE::NiPoint3       direction = targetToProjectile;
		direction.Unitize();
		RE::NiPoint3 targetVelocityDirection = a_targetVelocity;
		targetVelocityDirection.Unitize();

		const float cosTheta = (targetSpeedSquared > 0) ? direction.Dot(targetVelocityDirection) : 1.0f;

		bool  validSolutionFound = true;
		float t;

		if (ApproximatelyEqual(projectileSpeedSquared, targetSpeedSquared)) {
			// Evita la división por cero cuando objetivo y proyectil van a la
			// misma velocidad: cos(theta) <= 0 no tiene solución.
			if (cosTheta > 0) {
				t = 0.5f * distance / (targetSpeed * cosTheta);
			} else {
				validSolutionFound = false;
				t = 1;
			}
		} else {
			const float a = projectileSpeedSquared - targetSpeedSquared;
			const float b = 2.0f * distance * targetSpeed * cosTheta;
			const float c = -distanceSquared;
			const float discriminant = b * b - 4.0f * a * c;

			if (discriminant < 0) {
				validSolutionFound = false;
				t = 1;
			} else {
				const float uglyNumber = std::sqrtf(discriminant);
				const float t0 = 0.5f * (-b + uglyNumber) / a;
				const float t1 = 0.5f * (-b - uglyNumber) / a;

				// El menor tiempo positivo: el impacto más temprano posible.
				t = std::fmin(t0, t1);
				if (t < FLT_EPSILON) {
					t = std::fmax(t0, t1);
				}

				if (t < FLT_EPSILON) {
					// Sin solución real: tiro a ciegas hacia la posición
					// futura del objetivo.
					validSolutionFound = false;
					t = 1;
				}
			}
		}

		a_projectileVelocity = a_targetVelocity + (-targetToProjectile / t);

		if (!validSolutionFound) {
			a_projectileVelocity.Unitize();
			a_projectileVelocity *= projectileSpeed;
		}

		if (!ApproximatelyEqual(a_gravity, 0.f)) {
			const float netFallDistance = (a_projectileVelocity * t).z;
			const float gravityCompensationSpeed = (netFallDistance + 0.5f * a_gravity * t * t) / t;
			a_projectileVelocity.z = gravityCompensationSpeed;
		}

		return validSolutionFound;
	}
}
