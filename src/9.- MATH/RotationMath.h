// Rotaciones: grados a radianes, slerp, rotación y transformación local, curva suave y base a partir de un eje de
// avance. Las usan WeaponAnimation, WeaponTrail, WeaponVFX, Throw, Return y LightningDash.

#pragma once

#include <numbers>

namespace Math
{
	constexpr float DegreesToRadians(float a_degrees)
	{
		return a_degrees * std::numbers::pi_v<float> / 180.0f;
	}

	// Interpolación esférica entre dos rotaciones (a_t sin acotar).
	RE::NiMatrix3 SlerpRotation(const RE::NiMatrix3& a_from, const RE::NiMatrix3& a_to, float a_t);

	// Rotación local que, bajo a_parentWorld (rotación pura), da a_desiredWorld.
	RE::NiMatrix3 LocalRotationFromWorld(const RE::NiMatrix3& a_parentWorld, const RE::NiMatrix3& a_desiredWorld);

	// Transformación local de a_node que lo deja en a_worldTransform bajo su padre actual (sin padre, la misma).
	// Para mover nodos de un efecto en espacio mundial (estela y chispas).
	RE::NiTransform LocalTransformFromWorld(const RE::NiAVObject& a_node, const RE::NiTransform& a_worldTransform);

	// Curva suave 0->1 (3t²-2t³), a_t acotado.
	float SmoothStep01(float a_t);

	// Rotación con a_forward en el eje Y local y a_up fijando el giro; a_roll gira después
	// alrededor de a_forward. Si a_up es casi paralelo se usa un eje del mundo.
	void SetRotationFromForwardUp(RE::NiMatrix3& a_matrix, const RE::NiPoint3& a_forward, const RE::NiPoint3& a_up, float a_roll);

	// Roll que hace coincidir la base con a_planeUp con la de a_desiredUp en a_forward.
	// La usa Return::BeginReturnMovement para orientar la estela hacia la mano.
	float ComputeRoll(const RE::NiPoint3& a_forward, const RE::NiPoint3& a_planeUp, const RE::NiPoint3& a_desiredUp);
}
