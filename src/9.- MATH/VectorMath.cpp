// Vectores -- ver VectorMath.h.

#include "9.- MATH/VectorMath.h"

namespace Math
{
	RE::NiPoint3 NormalizedOr(const RE::NiPoint3& a_vector, const RE::NiPoint3& a_fallback)
	{
		const float length = a_vector.Length();
		return length > 0.0f ? a_vector / length : a_fallback;
	}

	RE::NiPoint3 ToNiPoint3(const RE::hkVector4& a_vector)
	{
		alignas(16) float components[4]{};
		_mm_store_ps(components, a_vector.quad);
		return { components[0], components[1], components[2] };
	}
}
