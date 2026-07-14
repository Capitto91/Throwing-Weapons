// Gestiona todo el proceso de lanzamiento.
// Crea el proyectil réplica del arma, calcula posición inicial y aplica la fuerza
// inicial del lanzamiento.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

#include <functional>

namespace Throw
{
	struct LaunchCallbacks
	{
		// Réplica creada y lista para volar (handle inválido si nunca
		// llegó a crearse o a cargar su 3D). Pensado para que el llamante
		// guarde el handle y pueda recuperarla más tarde.
		std::function<void(RE::ObjectRefHandle)> onSpawned;

		// Token del bucle de tick que controla la réplica en cada momento
		// (vuelo parabólico, o el seguimiento de un actor clavado que lo
		// sustituye — ver Combat::BeginEmbeddedEffect). El llamante debe
		// guardar siempre el último token recibido: es lo único que
		// permite cancelar ese bucle desde fuera (p. ej. al pulsar el
		// botón de recuperar) sin destruir la réplica, ver
		// Physics::TickToken.
		std::function<void(Physics::TickToken)> onTickStarted;

		// Impacto detectado (superficie o actor, punto 6 de Mecanica del
		// arma.txt): la réplica ya ha dejado de moverse en el punto del
		// golpe. a_actor es un handle válido si el impacto fue contra un
		// actor (LaunchWeapon ya se ha encargado de aplicar
		// Combat::BeginEmbeddedEffect en ese caso), inválido si fue contra
		// una superficie.
		std::function<void(RE::ActorHandle a_actor)> onStuck;

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
