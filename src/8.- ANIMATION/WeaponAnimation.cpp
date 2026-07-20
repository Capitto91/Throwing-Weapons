// Implementación de las animaciones del arma.
// Controla rotaciones, alineaciones y efectos visuales asociados.

#include "8.- ANIMATION/WeaponAnimation.h"

#include "1.- CORE/Constants.h"

#include <thread>

namespace Animation
{
	void TickSpin(RE::TESObjectREFR& a_refr, float a_elapsedSeconds)
	{
		auto* root = a_refr.Get3D();
		auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;
		if (!spinNode) {
			return;
		}

		spinNode->local.rotate.MakeRotation(Constants::kSpinAngularSpeed * a_elapsedSeconds, Constants::kSpinAxisLocal);
	}

	RE::NiTransform GetVisualTransform(RE::NiAVObject& a_root)
	{
		if (auto* spinNode = a_root.GetObjectByName(Constants::kWeaponSpinNodeName)) {
			return spinNode->world;
		}

		return a_root.world;
	}

	void PlayCaptureTransition(RE::NiAVObject& a_handNode, RE::NiAVObject& a_replicaRoot, std::function<void()> a_onComplete)
	{
		auto* handNode = a_handNode.AsNode();
		if (!handNode) {
			logs::warn("Animation::PlayCaptureTransition: el nodo de la mano no es un NiNode, se omite la transición.");
			a_onComplete();
			return;
		}

		// NiObject::Clone() ya construye internamente el NiCloningProcess
		// correcto -- no hay ningún uso de construirlo a mano en todo
		// commonlibsse-ng, y sus campos no están documentados en ningún
		// sitio (ver PLAN-mejoras-kratos.md). Se envuelve en NiPointer de
		// inmediato, mismo idioma ya usado en este proyecto
		// (WeaponTrail.cpp con su propio efecto) para no depender de
		// memoria sobre si Clone() ya entrega una referencia propia.
		RE::NiPointer<RE::NiObject> clonedObj(a_replicaRoot.Clone());
		auto*                       clonedNode = clonedObj ? clonedObj->AsNode() : nullptr;
		if (!clonedNode) {
			logs::warn("Animation::PlayCaptureTransition: no se pudo clonar el modelo de la réplica, se omite la transición.");
			a_onComplete();
			return;
		}

		// AttachChild añade su propia referencia al array children del
		// padre -- tras esto el objeto legítimamente tiene ≥2 dueños (este
		// NiPointer local y el propio nodo padre), es lo esperado.
		handNode->AttachChild(clonedNode, false);

		// El local heredado del original es relativo a la celda, no al
		// hueso de la mano -- hay que sobrescribirlo explícitamente, mismo
		// patrón ya usado en WeaponTrail.cpp (segmentBone->local = ...;
		// segmentBone->world = ...;, los dos escritos a mano). Rotación
		// local en identidad como punto de partida (ver
		// Constants::kCaptureTransitionLocalOffset).
		RE::NiTransform localTransform{};
		localTransform.translate = Constants::kCaptureTransitionLocalOffset;
		clonedNode->local = localTransform;
		clonedNode->world = handNode->world * localTransform;

		// Se guarda el clon vía NiPointer (mantiene el objeto vivo durante
		// la espera) y se desengancha por su propio puntero a padre en vez
		// de depender de a_handNode, que podría no seguir siendo válido
		// varios cientos de ms después (mismo motivo de cautela que ya
		// aplica en el resto del proyecto a los punteros crudos de
		// larga vida).
		RE::NiPointer<RE::NiNode> clone(clonedNode);
		std::thread([clone, onComplete = std::move(a_onComplete)]() {
			std::this_thread::sleep_for(Constants::kCaptureTransitionDuration);
			SKSE::GetTaskInterface()->AddTask([clone, onComplete]() {
				if (auto* parent = clone->parent) {
					parent->DetachChild(clone.get());
				}
				onComplete();
			});
		}).detach();
	}
}
