// Implementación de las animaciones del arma.
// Controla rotaciones, alineaciones y efectos visuales asociados.

#include "8.- ANIMATION/WeaponAnimation.h"

#include "1.- CORE/Constants.h"

#include <thread>

namespace Animation
{
	namespace
	{
		// El controlador vive en un nodo hijo dedicado
		// (Constants::kWeaponSpinNodeName), no en el nodo raíz -- ese lo
		// reescribe el propio código cada tick (SetAngle/Update3DPosition)
		// para mover la réplica, y competiría con el controlador si
		// compartieran nodo. Start()/Stop() ya son virtuales en la propia
		// clase base NiTimeController, así que no hace falta ningún
		// NiControllerManager ni buscar por nombre de secuencia -- solo
		// recorrer la lista enlazada de controladores del nodo (GetNext())
		// hasta dar con el de transformación, por si hubiera otros
		// (brillo, textura...).
		RE::NiTimeController* FindTransformController(RE::TESObjectREFR& a_refr)
		{
			auto* root = a_refr.Get3D();
			auto* spinNode = root ? root->GetObjectByName(Constants::kWeaponSpinNodeName) : nullptr;

			for (auto* controller = spinNode ? spinNode->GetControllers() : nullptr; controller; controller = controller->GetNext()) {
				if (controller->IsTransformController()) {
					return controller;
				}
			}

			return nullptr;
		}

		void RetryStartSpin(RE::ObjectRefHandle a_handle, int a_attemptsLeft)
		{
			auto refr = a_handle.get();
			if (!refr) {
				return;
			}

			if (auto* controller = FindTransformController(*refr)) {
				controller->Start(0.0f);
				return;
			}

			if (a_attemptsLeft <= 0) {
				logs::warn("Animation::StartSpinWhenReady: agotados los reintentos, el nodo '{}' nunca llegó a cargar.", Constants::kWeaponSpinNodeName);
				return;
			}

			// Mismo patrón que Physics::SpawnReplica: un hilo aparte que
			// duerme y reencola en el hilo principal, nunca AddTask
			// reentrante desde dentro de la propia tarea que se ejecuta.
			std::thread([a_handle, a_attemptsLeft]() {
				std::this_thread::sleep_for(Constants::kTickInterval);
				SKSE::GetTaskInterface()->AddTask([a_handle, a_attemptsLeft]() {
					RetryStartSpin(a_handle, a_attemptsLeft - 1);
				});
			}).detach();
		}
	}

	void StartSpin(RE::TESObjectREFR& a_refr)
	{
		auto* controller = FindTransformController(a_refr);
		if (!controller) {
			logs::warn("Animation::StartSpin: la réplica no tiene el nodo '{}' con su NiTransformController todavía (NIF sin la animación de giro).", Constants::kWeaponSpinNodeName);
			return;
		}

		controller->Start(0.0f);
	}

	void StartSpinWhenReady(RE::ObjectRefHandle a_handle)
	{
		RetryStartSpin(a_handle, Constants::kWeaponSpinNodeWaitAttempts);
	}

	void StopSpin(RE::TESObjectREFR& a_refr)
	{
		if (auto* controller = FindTransformController(a_refr)) {
			controller->Stop();
		}
	}

	RE::NiTransform GetVisualTransform(RE::NiAVObject& a_root)
	{
		if (auto* spinNode = a_root.GetObjectByName(Constants::kWeaponSpinNodeName)) {
			return spinNode->world;
		}

		return a_root.world;
	}
}
