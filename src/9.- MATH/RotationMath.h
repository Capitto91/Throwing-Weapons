// Biblioteca matemática para transiciones de rotación suaves.
// Usada para fundir la orientación real del arma (capturada del modelo
// equipado o de un hueso) con el giro calculado por código durante el
// vuelo -- ver 8.- ANIMATION/WeaponAnimation, punto 10 de
// "Mecanica del arma.txt".

#pragma once

namespace Math
{
	// Interpolación esférica (camino más corto) entre dos rotaciones,
	// expresadas como NiMatrix3 -- conversión a cuaternión y vuelta por
	// dentro (RE::NiQuaternion ya expone matriz<->cuaternión, ver
	// RE/N/NiQuaternion.h, pero no un Slerp propio). a_t fuera de [0,1] no
	// se acota -- responsabilidad del llamante (mismo criterio que
	// Math::EvaluateQuadraticBezier).
	RE::NiMatrix3 SlerpRotation(const RE::NiMatrix3& a_from, const RE::NiMatrix3& a_to, float a_t);

	// Rotación local que, compuesta con a_parentWorld (world = parent *
	// local -- convención ya usada en todo el proyecto, ver
	// 8.- ANIMATION/WeaponAnimation), reproduce exactamente a_desiredWorld.
	// a_parentWorld debe ser una rotación pura (ortonormal, su inversa es
	// su transpuesta) -- válido para cualquier NiMatrix3::world.rotate,
	// nunca para una matriz con escala.
	RE::NiMatrix3 LocalRotationFromWorld(const RE::NiMatrix3& a_parentWorld, const RE::NiMatrix3& a_desiredWorld);

	// Rotación de ángulo mínimo que lleva a_from a coincidir exactamente
	// con a_to (ambos vectores del mundo, no hace falta normalizarlos de
	// antemano). El giro alrededor del propio eje a_to queda sin
	// determinar a propósito -- no hay una única solución posible para
	// eso, válido para alinear un eje del modelo con una dirección sin
	// importar el "roll" resultante (ver
	// Animation::ComputeImpactAlignment). Si a_from y a_to son
	// antiparalelos, el eje de giro es ambiguo en teoría -- se elige uno
	// perpendicular arbitrario, caso límite no esperado en la práctica (el
	// arma no llega nunca exactamente de espaldas a su propia dirección de
	// vuelo).
	RE::NiMatrix3 ShortestArcRotation(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to);

	// Curva suave 0->1 (3t²-2t³), a_t fuera de [0,1] se acota -- extraída
	// aquí porque WeaponAnimation la necesita en varios sitios distintos
	// para el mismo tipo de transición de fundido (ver
	// Constants::kSpinRampDuration/kSpinStraightenDuration/
	// kImpactStraightenDuration).
	float SmoothStep01(float a_t);
}
