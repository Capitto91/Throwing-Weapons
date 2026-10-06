// Rotaciones -- ver RotationMath.h.

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

		// Camino más corto: -q es la misma rotación.
		float dot = from.Dot(to);
		if (dot < 0.0f) {
			to.Neg();
			dot = -dot;
		}

		// Casi paralelos: interpolación lineal para evitar dividir por ~0.
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

	RE::NiTransform LocalTransformFromWorld(const RE::NiAVObject& a_node, const RE::NiTransform& a_worldTransform)
	{
		if (auto* parent = a_node.parent) {
			return parent->world.Invert() * a_worldTransform;
		}

		return a_worldTransform;
	}

	float SmoothStep01(float a_t)
	{
		const float t = std::clamp(a_t, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	void SetRotationFromForwardUp(RE::NiMatrix3& a_matrix, const RE::NiPoint3& a_forward, const RE::NiPoint3& a_up, float a_roll)
	{
		// "right" perpendicular a a_forward y a_up; si degenera, eje del mundo de respaldo.
		RE::NiPoint3 right = a_up.Cross(a_forward);
		float        rightLength = right.Length();
		if (rightLength < 1.0e-4f) {
			const RE::NiPoint3 fallbackUp = std::abs(a_forward.z) < 0.99f ? RE::NiPoint3{ 0.0f, 0.0f, 1.0f } : RE::NiPoint3{ 1.0f, 0.0f, 0.0f };
			right = fallbackUp.Cross(a_forward);
			rightLength = right.Length();
		}
		right = rightLength > 0.0f ? right / rightLength : RE::NiPoint3{ 1.0f, 0.0f, 0.0f };

		// Ya unitario: sin normalizar.
		const RE::NiPoint3 up = a_forward.Cross(right);

		// a_roll gira (right, up) alrededor de a_forward.
		const float        cosRoll = std::cos(a_roll);
		const float        sinRoll = std::sin(a_roll);
		const RE::NiPoint3 rolledRight = right * cosRoll + up * sinRoll;
		const RE::NiPoint3 rolledUp = up * cosRoll - right * sinRoll;

		// Columnas: X = right, Y = a_forward, Z = up.
		a_matrix.entry[0][0] = rolledRight.x;
		a_matrix.entry[0][1] = a_forward.x;
		a_matrix.entry[0][2] = rolledUp.x;
		a_matrix.entry[1][0] = rolledRight.y;
		a_matrix.entry[1][1] = a_forward.y;
		a_matrix.entry[1][2] = rolledUp.y;
		a_matrix.entry[2][0] = rolledRight.z;
		a_matrix.entry[2][1] = a_forward.z;
		a_matrix.entry[2][2] = rolledUp.z;
	}

	float ComputeRoll(const RE::NiPoint3& a_forward, const RE::NiPoint3& a_planeUp, const RE::NiPoint3& a_desiredUp)
	{
		RE::NiMatrix3 planeBasis;
		RE::NiMatrix3 desiredBasis;
		SetRotationFromForwardUp(planeBasis, a_forward, a_planeUp, 0.0f);
		SetRotationFromForwardUp(desiredBasis, a_forward, a_desiredUp, 0.0f);

		const RE::NiPoint3 planeRight = planeBasis.GetVectorX();
		const RE::NiPoint3 planeUpAxis = planeBasis.GetVectorZ();
		const RE::NiPoint3 desiredRight = desiredBasis.GetVectorX();

		return std::atan2(planeUpAxis.Dot(desiredRight), planeRight.Dot(desiredRight));
	}
}
