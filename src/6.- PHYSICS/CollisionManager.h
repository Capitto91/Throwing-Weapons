// Colisión por raycast (RE::TES::Pick) para el vuelo y el punto de mira.

#pragma once

namespace Collision
{
	struct HitResult
	{
		bool               hit{ false };
		RE::NiPoint3       point{};
		RE::TESObjectREFR* target{ nullptr };            // referencia golpeada, si se pudo resolver (nullptr si no).
		RE::COL_LAYER      layer{ RE::COL_LAYER::kUnidentified };  // capa de colisión de lo golpeado.
		float              fraction{ 0.0f };              // 0-1 a lo largo de a_from->a_to; ver SweepRaycast.
		RE::NiPoint3       normal{};                      // normal de la superficie golpeada (unitaria, espacio del mundo, hkpShapeRayCastCollectorOutput::normal).
	};

	// Rayo de a_from a a_to; devuelve el impacto, ignorando a_ignore1/a_ignore2.
	// a_rayLayer: capa del rayo (kProjectile por defecto).
	HitResult Raycast(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, RE::TESObjectREFR* a_ignore1 = nullptr, RE::TESObjectREFR* a_ignore2 = nullptr, RE::COL_LAYER a_rayLayer = RE::COL_LAYER::kProjectile);

	// Rayo probado con kProjectile y, si no toca nada, con kLineOfSight.
	HitResult RaycastSolid(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, RE::TESObjectREFR* a_ignore1 = nullptr, RE::TESObjectREFR* a_ignore2 = nullptr);

	// Colisión en vuelo: varios rayos en cruz separados a_radius; gana el primer impacto.
	// El punto devuelto se recalcula sobre la línea central.
	HitResult SweepRaycast(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_radius, RE::TESObjectREFR* a_ignore1 = nullptr, RE::TESObjectREFR* a_ignore2 = nullptr);
}
