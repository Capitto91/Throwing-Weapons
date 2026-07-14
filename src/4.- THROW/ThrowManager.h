// Gestiona todo el proceso de lanzamiento.
// Crea el proyectil réplica del arma, calcula posición inicial y aplica la fuerza
// inicial del lanzamiento.

#pragma once

#include <functional>

namespace Throw
{
	struct LaunchCallbacks
	{
		// Réplica creada y lista para volar (handle inválido si nunca
		// llegó a crearse o a cargar su 3D). Pensado para que el llamante
		// guarde el handle y pueda recuperarla más tarde.
		std::function<void(RE::ObjectRefHandle)> onSpawned;

		// Impacto detectado (superficie o actor, punto 6 de Mecanica del
		// arma.txt): la réplica ya ha dejado de moverse en el punto del
		// golpe. El comportamiento específico según lo que se golpeó
		// (daño, parálisis, cómo engancharse a un actor) llega en la
		// Fase 5 — por ahora cualquier impacto se trata igual.
		std::function<void()> onStuck;

		// Distancia máxima superada sin impactar, o caída al agua (punto
		// 5): la réplica sigue existiendo pero ha dejado de moverse,
		// pendiente de que el llamante decida qué hacer (recuperación
		// automática).
		std::function<void()> onAutoRecall;
	};

	// Lanza la réplica visual del arma desde el nodo WEAPON de la mano
	// derecha de a_shooter, con trayectoria parabólica propia (velocidad y
	// gravedad constantes en Constants.h, sin depender de Havok ni de
	// ningún formulario Projectile) hacia el punto real al que apunta la
	// mirilla (corrección de paralaje cámara/mano, ver el plan de
	// integración) y colisión por raycast cada tick
	// (6.- PHYSICS/CollisionManager).
	void LaunchWeapon(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon, LaunchCallbacks a_callbacks);
}
