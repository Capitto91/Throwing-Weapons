// Implementación del sistema de colisiones.
// Procesa impactos y comunica los resultados al resto de sistemas.

#include "6.- PHYSICS/CollisionManager.h"

#include <array>
#include <cmath>

namespace Collision
{
	namespace
	{
		// No toda capa que el raycast puede golpear es geometría física de
		// verdad: Skyrim también usa Havok para volúmenes de detección/IA
		// (p. ej. ActorZone) sin ninguna referencia sólida detrás.
		// Confirmado en el juego (lanzamiento al cielo, sin nada visible
		// alrededor): un impacto contra ActorZone se aceptaba como si
		// fuera una pared. Lista blanca de capas que sí representan algo
		// contra lo que el arma debe detenerse de verdad — el resto se
		// descarta igual que un impacto ignorado.
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
			// La cápsula de movimiento normal de un actor de pie (no en
			// ragdoll) puede estar aquí en vez de en kBiped — añadida tras
			// detectar en el juego que los NPC se atravesaban de forma
			// intermitente; a confirmar si esto lo arregla del todo.
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
		auto* tes = RE::TES::GetSingleton();
		if (!tes) {
			return {};
		}

		const float scale = RE::bhkWorld::GetWorldScale();

		RE::bhkPickData pickData;
		pickData.rayInput.from = RE::hkVector4(a_from * scale);
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

		const RE::NiPoint3 point = a_from + (a_to - a_from) * pickData.rayOutput.hitFraction;

		const bool selfOrShooter = target && (target == a_ignore1 || target == a_ignore2);
		const bool accepted = !selfOrShooter && IsSolidLayer(layer);

		// Diagnóstico (Fase 4): el comportamiento del raycast contra
		// paredes/árboles/actores ha resultado poco fiable en las primeras
		// pruebas en el juego; este log deja constancia de cada impacto
		// real detectado por Havok —aceptado o descartado, y por qué— con
		// su capa y referencia resuelta, para diagnosticar con datos en
		// vez de a ciegas.
		logs::info(
			"Collision::Raycast: impacto en ({:.1f},{:.1f},{:.1f}), rayo={}, capa={}, referencia=\"{}\", {}",
			point.x, point.y, point.z,
			a_rayLayer,
			layer,
			target ? target->GetName() : "sin resolver",
			accepted ? "aceptado" : (selfOrShooter ? "ignorado (lanzador/réplica)" : "ignorado (capa no sólida)"));

		if (!accepted) {
			return {};
		}

		return HitResult{ true, point, target, layer, pickData.rayOutput.hitFraction };
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
		const auto segment = a_to - a_from;
		const float length = segment.Length();
		if (length <= 0.0f) {
			return RaycastSolid(a_from, a_to, a_ignore1, a_ignore2);
		}

		const auto forward = segment / length;

		// Base perpendicular a la dirección de vuelo, evitando el caso
		// degenerado de un vuelo casi vertical (paralelo al eje Z, usado
		// como referencia por defecto).
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

			// Todos los rayos del barrido son paralelos y de la misma
			// longitud (solo trasladados), así que su fracción a lo largo
			// del recorrido es directamente comparable entre ellos, sin
			// necesidad de recalcular distancias.
			if (!closest.hit || hit.fraction < closest.fraction) {
				closest = hit;
				// El punto se recalcula sobre la línea central
				// (a_from->a_to), no sobre el rayo desviado que detectó el
				// impacto: de lo contrario el arma se clavaría hasta
				// a_radius unidades a un lado de la trayectoria real
				// (comprobado en el juego).
				closest.point = a_from + segment * hit.fraction;
			}
		}

		return closest;
	}
}
