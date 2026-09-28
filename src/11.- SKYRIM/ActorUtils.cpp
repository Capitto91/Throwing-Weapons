// Utilidades sobre actores -- ver ActorUtils.h.

#include "11.- SKYRIM/ActorUtils.h"

#include "1.- CORE/Constants.h"

#include <limits>

namespace ActorUtils
{
	namespace
	{
		// Recorre el árbol 3D y guarda en a_best el nodo más cercano a a_worldPoint.
		void VisitNodes(RE::NiAVObject* a_node, const RE::NiPoint3& a_worldPoint, RE::NiAVObject*& a_best, float& a_bestDistanceSq)
		{
			if (!a_node) {
				return;
			}

			const float distanceSq = a_node->world.translate.GetSquaredDistance(a_worldPoint);
			if (distanceSq < a_bestDistanceSq) {
				a_bestDistanceSq = distanceSq;
				a_best = a_node;
			}

			if (auto* asNode = a_node->AsNode()) {
				for (auto& child : asNode->GetChildren()) {
					VisitNodes(child.get(), a_worldPoint, a_best, a_bestDistanceSq);
				}
			}
		}
	}

	bool IsThrowableWeaponEquipped(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return false;
		}

		// false = mano derecha / mano principal.
		const auto* rightHand = a_actor->GetEquippedObject(false);
		const auto* weapon = rightHand ? rightHand->As<RE::TESObjectWEAP>() : nullptr;

		return weapon && weapon->HasKeywordString(Constants::kThrowableWeaponKeyword);
	}

	RE::BSFixedString FindNearestBoneName(RE::Actor* a_actor, const RE::NiPoint3& a_worldPoint)
	{
		auto* root = a_actor ? a_actor->Get3D() : nullptr;
		if (!root) {
			return {};
		}

		RE::NiAVObject* best = nullptr;
		float           bestDistanceSq = (std::numeric_limits<float>::max)();
		VisitNodes(root, a_worldPoint, best, bestDistanceSq);

		return best ? best->name : RE::BSFixedString{};
	}
}
