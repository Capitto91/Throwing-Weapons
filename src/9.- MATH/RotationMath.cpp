// Implementación de cálculos de rotación.
// Gestiona quaterniones, orientación hacia objetivos y ajuste del mango.

#include "9.- MATH/RotationMath.h"

#include <cmath>

namespace Math
{
	void SetRotationMatrix(RE::NiMatrix3& a_matrix, float a_sacb, float a_cacb, float a_sb)
	{
		const float cb = std::sqrt(1.0f - a_sb * a_sb);
		const float ca = a_cacb / cb;
		const float sa = a_sacb / cb;

		a_matrix.entry[0][0] = ca;
		a_matrix.entry[0][1] = -a_sacb;
		a_matrix.entry[0][2] = sa * a_sb;
		a_matrix.entry[1][0] = sa;
		a_matrix.entry[1][1] = a_cacb;
		a_matrix.entry[1][2] = -ca * a_sb;
		a_matrix.entry[2][0] = 0.0f;
		a_matrix.entry[2][1] = a_sb;
		a_matrix.entry[2][2] = cb;
	}
}
