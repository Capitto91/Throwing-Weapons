// Cálculos del regreso: punto de control de la curva, lado de desvío y perfil de aceleración.

#pragma once

namespace Return
{
	// Eje "derecha" del actor ({1,0,0} sin 3D); decide hacia qué lado se curva el regreso.
	RE::NiPoint3 GetPlayerRightVector(RE::Actor* a_actor);

	// Punto de control de la Bezier entre a_start y a_end: anclado en a_anchorFraction de la línea
	// y desviado hacia a_rightVector (Constants::kReturnCurveLateralFraction, acotado).
	RE::NiPoint3 ComputeReturnControlPoint(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, const RE::NiPoint3& a_rightVector, float a_anchorFraction);

	// Coeficiente de aceleración para llegar a Constants::kReturnTargetArrivalSpeed
	// sin superar Constants::kReturnMaxDuration.
	float ComputeReturnAcceleration(float a_distance);

	// Distancia recorrida tras a_elapsedSeconds: d(t) = a/(n·(n-1))·t^n,
	// n = Constants::kReturnAccelerationExponent.
	float ComputeTraveledDistance(float a_acceleration, float a_elapsedSeconds);

	// Duración prevista para recorrer a_distance con a_acceleration.
	// La usa Return::BeginReturn para adelantar el sonido de atrape.
	float ComputeReturnDuration(float a_acceleration, float a_distance);

	// Aceleración para recorrer a_distance en a_targetDuration.
	// La usa BeginReturnMovement para alargar el vuelo si no hubo temblor.
	float ComputeReturnAccelerationForDuration(float a_distance, float a_targetDuration);
}
