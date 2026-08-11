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

	// Curva suave 0->1 (3t²-2t³), a_t fuera de [0,1] se acota -- extraída
	// aquí porque WeaponAnimation la necesita en varios sitios distintos
	// para el mismo tipo de transición de fundido (ver
	// Constants::kSpinRampDuration/kSpinStraightenLeadTime).
	float SmoothStep01(float a_t);

	// Construye a_matrix a partir de los senos/cosenos de un ángulo
	// compuesto (seno·coseno del ángulo A por el coseno de B, coseno de A
	// por coseno de B, y seno de B) -- forma de construir una matriz de
	// rotación directamente desde una dirección normalizada sin pasar por
	// ángulos de Euler explícitos. Usada por 8.- ANIMATION/WeaponTrail para
	// orientar cada segmento de la estela según la tangente de la curva de
	// Catmull-Rom en ese punto. Portado tal cual de Precision (Ershin, MIT
	// License, github.com/ersh1/Precision, src/Utils.h,
	// Utils::SetRotationMatrix).
	void SetRotationMatrix(RE::NiMatrix3& a_matrix, float a_sacb, float a_cacb, float a_sb);
}
