// Vectores: normalizar con valor de respaldo y convertir un vector de Havok.
// Los usan Throw, Return, LightningDash, Combat, la estela y las colisiones.

#pragma once

namespace Math
{
	// Eje vertical del mundo (Z hacia arriba).
	inline constexpr RE::NiPoint3 kWorldUp{ 0.0f, 0.0f, 1.0f };

	// a_vector normalizado; si mide 0, a_fallback (ya unitario o nulo, tal cual).
	[[nodiscard]] RE::NiPoint3 NormalizedOr(const RE::NiPoint3& a_vector, const RE::NiPoint3& a_fallback);

	// Componentes x, y, z de a_vector (sin escala de mundo: quien lo usa la aplica si hace falta).
	[[nodiscard]] RE::NiPoint3 ToNiPoint3(const RE::hkVector4& a_vector);
}
