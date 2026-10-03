// Lanzamiento: crea la réplica y la mueve en parábola hasta que impacta o cae al agua.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

#include <functional>

namespace Throw
{
	struct LaunchCallbacks
	{
		// Réplica lista para volar (handle inválido si no llegó a cargar).
		std::function<void(RE::ObjectRefHandle)> onSpawned;

		// Token del bucle que mueve la réplica (vuelo o seguimiento del actor clavado).
		// El llamante guarda el último para poder cancelarlo.
		std::function<void(Physics::TickToken)> onTickStarted;

		// Impacto: la réplica se ha detenido. a_actor es válido si se clavó en un actor
		// (ya con Combat::BeginEmbeddedEffect aplicado); si no, a_surfaceNormal es la normal de la superficie.
		std::function<void(RE::ActorHandle a_actor, const RE::NiPoint3& a_surfaceNormal)> onStuck;

		// Caída al agua: la réplica se ha detenido sin clavarse.
		std::function<void()> onAutoRecall;
	};

	// Lanza la réplica desde la mano derecha de a_shooter hacia donde apunta la mirilla.
	// La llama WeaponManager::ThrowWeapon; avisa por los callbacks de arriba.
	void LaunchWeapon(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon, LaunchCallbacks a_callbacks);
}
