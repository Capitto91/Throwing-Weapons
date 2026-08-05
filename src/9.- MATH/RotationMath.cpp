// Implementación de las transiciones de rotación suaves.

#include "9.- MATH/RotationMath.h"

#include <algorithm>
#include <cmath>

namespace Math
{
	namespace
	{
		RE::NiQuaternion NormalizedQuaternion(const RE::NiMatrix3& a_matrix)
		{
			RE::NiQuaternion q(a_matrix);
			const float      lengthSq = q.Dot(q);
			if (lengthSq > 0.0f) {
				const float invLength = 1.0f / std::sqrt(lengthSq);
				q.w *= invLength;
				q.x *= invLength;
				q.y *= invLength;
				q.z *= invLength;
			}
			return q;
		}
	}

	RE::NiMatrix3 SlerpRotation(const RE::NiMatrix3& a_from, const RE::NiMatrix3& a_to, float a_t)
	{
		const RE::NiQuaternion from = NormalizedQuaternion(a_from);
		RE::NiQuaternion       to = NormalizedQuaternion(a_to);

		// Camino más corto: -q representa la misma rotación que q, pero
		// interpolar hacia el hemisferio opuesto daría la vuelta larga.
		float dot = from.Dot(to);
		if (dot < 0.0f) {
			to.Neg();
			dot = -dot;
		}

		// Casi paralelos: la fórmula trigonométrica de más abajo divide
		// por sin(theta), que tiende a 0 aquí -- interpolación lineal (y
		// renormalizar) es indistinguible visualmente y evita
		// inestabilidad numérica.
		float w, x, y, z;
		if (dot > 0.9995f) {
			w = from.w + (to.w - from.w) * a_t;
			x = from.x + (to.x - from.x) * a_t;
			y = from.y + (to.y - from.y) * a_t;
			z = from.z + (to.z - from.z) * a_t;
		} else {
			const float theta0 = std::acos(std::clamp(dot, -1.0f, 1.0f));
			const float theta = theta0 * a_t;
			const float sinTheta0 = std::sin(theta0);
			const float s0 = std::cos(theta) - dot * std::sin(theta) / sinTheta0;
			const float s1 = std::sin(theta) / sinTheta0;

			w = s0 * from.w + s1 * to.w;
			x = s0 * from.x + s1 * to.x;
			y = s0 * from.y + s1 * to.y;
			z = s0 * from.z + s1 * to.z;
		}

		RE::NiQuaternion result(w, x, y, z);
		const float      lengthSq = result.Dot(result);
		if (lengthSq > 0.0f) {
			const float invLength = 1.0f / std::sqrt(lengthSq);
			result.w *= invLength;
			result.x *= invLength;
			result.y *= invLength;
			result.z *= invLength;
		}

		return result.ToRotation();
	}

	RE::NiMatrix3 LocalRotationFromWorld(const RE::NiMatrix3& a_parentWorld, const RE::NiMatrix3& a_desiredWorld)
	{
		// a_parentWorld ortonormal -> su inversa es su transpuesta.
		return a_parentWorld.Transpose() * a_desiredWorld;
	}

	RE::NiMatrix3 ShortestArcRotation(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to)
	{
		const float fromLength = a_from.Length();
		const float toLength = a_to.Length();
		if (fromLength <= 0.0f || toLength <= 0.0f) {
			return RE::NiMatrix3{};  // identidad -- entrada degenerada
		}

		const RE::NiPoint3 from = a_from / fromLength;
		const RE::NiPoint3 to = a_to / toLength;

		const float cosAngle = std::clamp(from.Dot(to), -1.0f, 1.0f);

		// Prácticamente ya alineados: identidad, evita normalizar un eje
		// casi nulo más abajo.
		if (cosAngle > 0.99999f) {
			return RE::NiMatrix3{};
		}

		RE::NiPoint3 axis = from.Cross(to);

		// Antiparalelos: el producto cruzado es (casi) nulo -- cualquier
		// eje perpendicular a "from" sirve, ver comentario del header.
		if (cosAngle < -0.99999f) {
			const RE::NiPoint3 arbitrary = std::abs(from.x) < 0.9f ? RE::NiPoint3{ 1.0f, 0.0f, 0.0f } : RE::NiPoint3{ 0.0f, 1.0f, 0.0f };
			axis = from.Cross(arbitrary);
		}

		const float axisLength = axis.Length();
		if (axisLength <= 0.0f) {
			return RE::NiMatrix3{};
		}

		RE::NiMatrix3 result;
		result.MakeRotation(std::acos(cosAngle), axis / axisLength);
		return result;
	}

	float SmoothStep01(float a_t)
	{
		const float t = std::clamp(a_t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}
}
