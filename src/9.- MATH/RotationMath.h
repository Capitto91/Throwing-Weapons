// Herramientas matemáticas para rotaciones.
// Calcula orientaciones, direcciones y alineaciones del arma.

#pragma once

namespace Math
{
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
