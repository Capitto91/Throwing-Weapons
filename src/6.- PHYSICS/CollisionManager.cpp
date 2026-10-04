// Colisión por raycast -- ver CollisionManager.h.

#include "6.- PHYSICS/CollisionManager.h"

#include <array>
#include <cmath>

namespace Collision
{
	namespace
	{
		// Impactos descartados que Raycast puede saltar seguidos, y avance tras cada uno para reanudar el rayo
		// fuera de lo que golpeó (un rayo que empieza dentro de una forma no la vuelve a tocar).
		constexpr int   kRaycastMaxSkippedHits = 8;
		constexpr float kRaycastSkipDistance = 1.0f;

		// Capas que cuentan como impacto sólido; el resto (p. ej. ActorZone) se ignora.
		bool IsSolidLayer(RE::COL_LAYER a_layer)
		{
			switch (a_layer) {
			case RE::COL_LAYER::kStatic:
			case RE::COL_LAYER::kAnimStatic:
			case RE::COL_LAYER::kTerrain:
			case RE::COL_LAYER::kGround:
			case RE::COL_LAYER::kTrees:
			case RE::COL_LAYER::kProps:
			case RE::COL_LAYER::kClutter:
			case RE::COL_LAYER::kTrap:
			case RE::COL_LAYER::kBiped:
			case RE::COL_LAYER::kDeadBip:
			case RE::COL_LAYER::kBipedNoCC:
			// Cápsula de movimiento de un actor de pie.
			case RE::COL_LAYER::kCharController:
			case RE::COL_LAYER::kWeapon:
			case RE::COL_LAYER::kInvisibleWall:
			case RE::COL_LAYER::kDebrisSmall:
			case RE::COL_LAYER::kDebrisLarge:
				return true;
			default:
				return false;
			}
		}
	}

	HitResult Raycast(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, RE::TESObjectREFR* a_ignore1, RE::TESObjectREFR* a_ignore2, RE::COL_LAYER a_rayLayer)
	{
		auto*       tes = RE::TES::GetSingleton();
		const float length = (a_to - a_from).Length();
		if (!tes || length <= 0.0f) {
			return {};
		}

		const float        scale = RE::bhkWorld::GetWorldScale();
		const RE::NiPoint3 direction = (a_to - a_from) / length;

		// Pick solo da el impacto más cercano: si se descarta (a_ignore1/a_ignore2 o capa no sólida), el rayo
		// sigue desde justo después de él.
		RE::NiPoint3 from = a_from;
		for (int attempt = 0; attempt < kRaycastMaxSkippedHits; ++attempt) {
			RE::bhkPickData pickData;
			pickData.rayInput.from = RE::hkVector4(from * scale);
			pickData.rayInput.to = RE::hkVector4(a_to * scale);
			pickData.rayInput.filterInfo.SetCollisionLayer(a_rayLayer);

			tes->Pick(pickData);

			if (!pickData.rayOutput.HasHit()) {
				return {};
			}

			auto* target = pickData.rayOutput.rootCollidable ?
			                   RE::TESHavokUtilities::FindCollidableRef(*pickData.rayOutput.rootCollidable) :
			                   nullptr;

			const auto layer = pickData.rayOutput.rootCollidable ?
			                       pickData.rayOutput.rootCollidable->broadPhaseHandle.collisionFilterInfo.GetCollisionLayer() :
			                       RE::COL_LAYER::kUnidentified;

			const RE::NiPoint3 point = from + (a_to - from) * pickData.rayOutput.hitFraction;

			const bool selfOrShooter = target && (target == a_ignore1 || target == a_ignore2);
			if (!selfOrShooter && IsSolidLayer(layer)) {
				// La normal ya es unitaria, sin escala de mundo; la fracción, sobre el rayo original.
				alignas(16) float normal[4]{};
				_mm_store_ps(normal, pickData.rayOutput.normal.quad);

				const float fraction = (point - a_from).Length() / length;
				return HitResult{ true, point, target, layer, fraction, RE::NiPoint3{ normal[0], normal[1], normal[2] } };
			}

			from = point + direction * kRaycastSkipDistance;
			if ((from - a_from).Length() >= length) {
				return {};
			}
		}

		return {};
	}

	HitResult RaycastSolid(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, RE::TESObjectREFR* a_ignore1, RE::TESObjectREFR* a_ignore2)
	{
		if (auto hit = Raycast(a_from, a_to, a_ignore1, a_ignore2, RE::COL_LAYER::kProjectile); hit.hit) {
			return hit;
		}

		return Raycast(a_from, a_to, a_ignore1, a_ignore2, RE::COL_LAYER::kLineOfSight);
	}

	HitResult SweepRaycast(const RE::NiPoint3& a_from, const RE::NiPoint3& a_to, float a_radius, RE::TESObjectREFR* a_ignore1, RE::TESObjectREFR* a_ignore2)
	{
		const auto  segment = a_to - a_from;
		const float length = segment.Length();
		if (length <= 0.0f) {
			return RaycastSolid(a_from, a_to, a_ignore1, a_ignore2);
		}

		const auto forward = segment / length;

		// Base perpendicular al vuelo, evitando el caso casi vertical.
		RE::NiPoint3 reference{ 0.0f, 0.0f, 1.0f };
		if (std::abs(forward.Dot(reference)) > 0.99f) {
			reference = { 1.0f, 0.0f, 0.0f };
		}

		auto right = forward.Cross(reference);
		right = right / right.Length();
		auto up = right.Cross(forward);
		up = up / up.Length();

		const std::array<RE::NiPoint3, 5> offsets{
			RE::NiPoint3{ 0.0f, 0.0f, 0.0f },
			right * a_radius,
			right * -a_radius,
			up * a_radius,
			up * -a_radius,
		};

		HitResult closest;

		for (const auto& offset : offsets) {
			const auto hit = RaycastSolid(a_from + offset, a_to + offset, a_ignore1, a_ignore2);
			if (!hit.hit) {
				continue;
			}

			// Rayos paralelos y de igual longitud: sus fracciones se comparan directamente.
			if (!closest.hit || hit.fraction < closest.fraction) {
				closest = hit;
				// Punto sobre la línea central, no sobre el rayo desviado.
				closest.point = a_from + segment * hit.fraction;
			}
		}

		return closest;
	}
}
