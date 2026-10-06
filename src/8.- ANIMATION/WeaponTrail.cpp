// Estela -- ver WeaponTrail.h. Portado de AttackTrail::Update de Precision (ersh1, github.com/ersh1/Precision;
// licencia en README.md, Créditos); segmentos en línea recta entre las dos últimas muestras.

#include "8.- ANIMATION/WeaponTrail.h"

#include "1.- CORE/Constants.h"
#include "9.- MATH/CurveMath.h"
#include "9.- MATH/RotationMath.h"
#include "9.- MATH/VectorMath.h"

namespace Animation
{
	namespace
	{
		// Vida del efecto: margen amplio para cualquier vuelo.
		constexpr float kParticleLifetime = 10.0f;

		// Aparca todos los segmentos a escala 0.
		void ParkAllSegments(RE::NiNode& a_trailRootNode, const RE::NiTransform& a_parkedTransform)
		{
			auto&      segments = a_trailRootNode.GetChildren();
			const auto segmentCount = static_cast<std::uint32_t>(segments.size());
			for (std::uint32_t i = 0; i < segmentCount; ++i) {
				if (auto& segmentBone = segments[static_cast<std::uint16_t>(i)]) {
					segmentBone->local = Math::LocalTransformFromWorld(*segmentBone, a_parkedTransform);
					segmentBone->world = a_parkedTransform;
				}
			}
		}

		// Nombre del hueso i: "Bone001"...
		std::string BoneNameForOneBasedIndex(std::uint32_t a_oneBasedIndex)
		{
			std::string name = "Bone";
			if (a_oneBasedIndex < 10) {
				name += "00";
			} else if (a_oneBasedIndex < 100) {
				name += "0";
			}
			name += std::to_string(a_oneBasedIndex);
			return name;
		}

	}

	WeaponTrail::~WeaponTrail()
	{
		if (particle) {
			// Fuerza age >= lifetime para que el motor retire el efecto.
			particle->age += particle->lifetime;
		}
	}

	void WeaponTrail::Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset)
	{
		orderedSegments.clear();
		upReference = a_upReference;
		roll = a_roll;
		anchorWorldOffset = a_anchorWorldOffset;

		if (!a_cell) {
			// Sin celda no arranca.
			logs::warn("Animation::WeaponTrail::Start: a_cell es nulo, no se crea el efecto.");
			return;
		}

		const RE::NiPoint3 anchoredInitialPosition = a_initialPosition + anchorWorldOffset;

		// Nace a escala 0; Update la pone a 1 cuando su 3D está listo.
		particle = RE::NiPointer<RE::BSTempEffectParticle>(
			RE::BSTempEffectParticle::Spawn(a_cell, kParticleLifetime, Constants::kTrailEffectPath, RE::NiMatrix3{}, anchoredInitialPosition, 0.0f, 7, nullptr));

		if (!particle) {
			logs::warn("Animation::WeaponTrail::Start: no se pudo crear el efecto '{}'.", Constants::kTrailEffectPath);
		}
	}

	void WeaponTrail::SetRoll(float a_roll)
	{
		roll = a_roll;
	}

	void WeaponTrail::Update(const RE::NiPoint3& a_currentPosition, [[maybe_unused]] float a_deltaSeconds)
	{
		if (!particle || !particle->particleObject) {
			// Sin 3D todavía no hace nada.
			return;
		}

		// Aplica el offset de anclaje a la posición recibida.
		const RE::NiPoint3 anchoredPosition = a_currentPosition + anchorWorldOffset;

		const float distanceThisTick = history.empty() ? 0.0f : (anchoredPosition - history.back()).Length();
		history.emplace_back(anchoredPosition);
		totalDistance += distanceThisTick;

		auto* fadeNode = particle->particleObject->AsFadeNode();
		if (!fadeNode) {
			return;
		}

		fadeNode->GetRuntimeData().currentFade = 1.0f;

		// Escala del nodo raíz a 1; cada segmento controla la suya.
		particle->particleObject->local.scale = 1.0f;
		particle->particleObject->world.scale = 1.0f;

		auto* trailRoot = fadeNode->GetObjectByName(Constants::kTrailRootNodeName);
		auto* trailRootNode = trailRoot ? trailRoot->AsNode() : nullptr;
		if (!trailRootNode) {
			logs::warn("Animation::WeaponTrail::Update: el efecto '{}' no tiene el nodo '{}' (NIF sin la convención de estela esperada).", Constants::kTrailEffectPath, Constants::kTrailRootNodeName);
			return;
		}

		// Huesos resueltos por nombre una vez por Start.
		if (orderedSegments.empty()) {
			auto&      rawChildren = trailRootNode->GetChildren();
			const auto rawCount = static_cast<std::uint32_t>(rawChildren.size());

			bool allResolvedByName = rawCount > 0;
			for (std::uint32_t i = 1; i <= rawCount; ++i) {
				auto* bone = trailRootNode->GetObjectByName(BoneNameForOneBasedIndex(i));
				if (!bone) {
					allResolvedByName = false;
					break;
				}
				orderedSegments.emplace_back(bone);
			}

			if (!allResolvedByName) {
				logs::warn("Animation::WeaponTrail::Update: no se pudieron resolver los {} huesos de '{}' por nombre (convención 'BoneNNN' no coincide) -- usando el orden crudo del archivo como respaldo, con riesgo de costuras mal conectadas.", rawCount, Constants::kTrailRootNodeName);
				orderedSegments.clear();
				orderedSegments.reserve(rawCount);
				for (std::uint32_t i = 0; i < rawCount; ++i) {
					orderedSegments.emplace_back(rawChildren[static_cast<std::uint16_t>(i)]);
				}
			}
		}

		auto&      segments = orderedSegments;
		const auto segmentCount = static_cast<std::uint32_t>(segments.size());
		if (segmentCount == 0) {
			return;
		}

		// Con menos de 2 muestras se aparcan los segmentos.
		if (history.size() < 2) {
			RE::NiTransform parkedTransform;
			parkedTransform.translate = history.back();
			parkedTransform.scale = 0.0f;
			ParkAllSegments(*trailRootNode, parkedTransform);
			return;
		}

		// Últimas dos muestras del historial.
		auto p2It = history.rbegin();
		auto p1It = p2It + 1;

		const auto& ip1 = *p1It;
		const auto& ip2 = *p2It;

		// Dirección de avance de los segmentos de este tick (ip2-ip1, negada por el sentido del NIF).
		const auto segmentAxis = -Math::NormalizedOr(ip2 - ip1, RE::NiPoint3{ 0.0f, 1.0f, 0.0f });

		float      segmentsToAdd = segmentsToAddRemainder + distanceThisTick / Constants::kTrailSegmentSpacing;
		const auto segmentsToAddTrunc = static_cast<std::uint32_t>(segmentsToAdd);
		segmentsToAddRemainder = segmentsToAdd - static_cast<float>(segmentsToAddTrunc);

		// Recicla los segmentos que quedaron kTrailLength atrás o fuerza hueco para los nuevos.
		if (!segmentDistances.empty()) {
			std::uint32_t segmentsToMove = 0;
			for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
				if (i < segmentDistances.size() && totalDistance - segmentDistances[i] > Constants::kTrailLength) {
					++segmentsToMove;
				} else {
					break;
				}
			}

			const std::uint32_t totalSegments = currentBoneIdx + segmentsToAddTrunc - segmentsToMove;
			if (totalSegments >= segmentCount) {
				segmentsToMove += totalSegments - (segmentCount - 1);
			}
			// Sin std::min: Windows.h define la macro min.
			const auto distanceCount = static_cast<std::uint32_t>(segmentDistances.size());
			if (segmentsToMove > distanceCount) {
				segmentsToMove = distanceCount;
			}

			if (segmentsToMove > 0) {
				segmentDistances.erase(segmentDistances.begin(), segmentDistances.begin() + segmentsToMove);

				for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
					if (segmentCount > i + segmentsToMove) {
						// Copia local y world a la vez.
						auto& destSegment = segments[static_cast<std::uint16_t>(i)];
						auto& srcSegment = segments[static_cast<std::uint16_t>(i + segmentsToMove)];
						destSegment->local = srcSegment->local;
						destSegment->world = srcSegment->world;
					}
				}

				currentBoneIdx -= segmentsToMove;
			}
		}

		// Añade los segmentos nuevos interpolados entre las dos últimas muestras.
		if (segmentsToAdd > 0.0f) {
			for (std::uint32_t i = 0; i < segmentsToAddTrunc; ++i) {
				if (segmentCount <= currentBoneIdx) {
					break;
				}

				auto& segmentBone = segments[static_cast<std::uint16_t>(currentBoneIdx)];
				if (!segmentBone) {
					continue;
				}

				const float t = (static_cast<float>(i) + 1.0f) / segmentsToAdd;
				const auto  interpolatedPos = Math::Lerp(ip1, ip2, t);

				RE::NiTransform newTransform = segmentBone->world;
				Math::SetRotationFromForwardUp(newTransform.rotate, segmentAxis, upReference, roll);
				newTransform.translate = interpolatedPos;
				newTransform.scale = Constants::kTrailSegmentScale;

				segmentBone->local = Math::LocalTransformFromWorld(*segmentBone, newTransform);
				segmentBone->world = newTransform;

				segmentDistances.emplace_back(totalDistance - distanceThisTick * (1.0f - t));
				++currentBoneIdx;
			}
		}

		// Estrecha cada segmento según la distancia desde que se colocó (cono hacia la cola).
		for (std::uint32_t i = 0; i < currentBoneIdx; ++i) {
			if (i >= segmentDistances.size()) {
				break;
			}

			auto& segmentBone = segments[static_cast<std::uint16_t>(i)];
			if (!segmentBone) {
				continue;
			}

			float ageFraction = (totalDistance - segmentDistances[i]) / Constants::kTrailLength;
			ageFraction = ageFraction < 0.0f ? 0.0f : (ageFraction > 1.0f ? 1.0f : ageFraction);

			const float taperedScale = Constants::kTrailSegmentScale * (1.0f - ageFraction);
			segmentBone->local.scale = taperedScale;
			segmentBone->world.scale = taperedScale;
		}

		// Segmentos sin usar: en la posición actual a escala 0.
		if (currentBoneIdx < segmentCount) {
			RE::NiTransform worldTransform;
			worldTransform.translate = history.back();

			Math::SetRotationFromForwardUp(worldTransform.rotate, segmentAxis, upReference, roll);
			worldTransform.scale = 0.0f;

			const auto localTransform = Math::LocalTransformFromWorld(*segments[static_cast<std::uint16_t>(currentBoneIdx)], worldTransform);
			for (std::uint32_t i = currentBoneIdx; i < segmentCount; ++i) {
				if (auto& segmentBone = segments[static_cast<std::uint16_t>(i)]) {
					segmentBone->local = localTransform;
					segmentBone->world = worldTransform;
				}
			}
		}
	}
}
