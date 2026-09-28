// Rotaciones: slerp, rotación local, curva suave y base a partir de un eje de avance.
// Las usan WeaponAnimation, WeaponTrail y Return.

#pragma once

namespace Math
{
	// Interpolación esférica entre dos rotaciones (a_t sin acotar).
	RE::NiMatrix3 SlerpRotation(const RE::NiMatrix3& a_from, const RE::NiMatrix3& a_to, float a_t);

	// Rotación local que, bajo a_parentWorld (rotación pura), da a_desiredWorld.
	RE::NiMatrix3 LocalRotationFromWorld(const RE::NiMatrix3& a_parentWorld, const RE::NiMatrix3& a_desiredWorld);

	// Curva suave 0->1 (3t²-2t³), a_t acotado.
	float SmoothStep01(float a_t);

	// Rotación con a_forward en el eje Y local y a_up fijando el giro; a_roll gira después
	// alrededor de a_forward. Si a_up es casi paralelo se usa un eje del mundo.
	void SetRotationFromForwardUp(RE::NiMatrix3& a_matrix, const RE::NiPoint3& a_forward, const RE::NiPoint3& a_up, float a_roll);

	// Roll que hace coincidir la base con a_planeUp con la de a_desiredUp en a_forward.
	// La usa Return::BeginReturnMovement para orientar la estela hacia la mano.
	float ComputeRoll(const RE::NiPoint3& a_forward, const RE::NiPoint3& a_planeUp, const RE::NiPoint3& a_desiredUp);
}
