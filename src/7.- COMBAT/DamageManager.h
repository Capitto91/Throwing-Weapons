// Gestiona la aplicación de daño provocado por el arma.
// Se utiliza tanto durante el lanzamiento como durante el retorno.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

#include <functional>

namespace Combat
{
	// Carga la configuración de daño desde el INI
	// (Data/SKSE/Plugins/ThrowingWeapons.ini, sección [Damage]): modo
	// (Direct/Level) y sus parámetros. Debe llamarse una única vez al
	// arrancar (ver Events::Init).
	void Init();

	// Punto 6 de Mecanica del arma.txt: la réplica acaba de impactar y
	// detenerse sobre a_target (un actor), en vez de una superficie.
	// Aplica el golpe inicial (daño real, ver Init/Combat::Init para cómo
	// se calcula) y, si el objetivo puede paralizarse (ni dragón,
	// Actor::IsDragon(), ni inmune según la condición del propio hechizo,
	// comprobado indirectamente vía el estado de aturdimiento tras
	// concederlo), concede la habilidad de parálisis propia
	// (Constants::kEmbeddedParalysisSpell) y arranca un seguimiento
	// continuo: la réplica sigue la posición de a_target tick a tick
	// (aproximación por posición, sin enganche real a hueso — no hay API
	// para localizar el hueso más cercano a un punto, ver CLAUDE.md),
	// repite el daño cada Constants::kEmbeddedDamageInterval segundos, y
	// llama a a_onAutoRecall si el objetivo resulta inmune o al superar
	// Constants::kEmbeddedMaxDuration clavada. Si el objetivo puede
	// paralizarse, llama primero a a_onStuck (con el handle del actor)
	// para que el llamante registre el ciclo como "clavada".
	// a_onTickStarted recibe el token del nuevo bucle de seguimiento que
	// arranca aqui (ver Physics::TickToken): sustituye al bucle de vuelo
	// de Throw::LaunchWeapon, y el llamante debe quedarse con este token
	// nuevo para poder cancelarlo mas adelante (p. ej. al pulsar el boton
	// de recuperar).
	void BeginEmbeddedEffect(
		RE::Actor*                              a_attacker,
		RE::Actor*                              a_target,
		RE::ObjectRefHandle                     a_replicaHandle,
		std::function<void(RE::ActorHandle)>    a_onStuck,
		std::function<void()>                   a_onAutoRecall,
		std::function<void(Physics::TickToken)> a_onTickStarted);

	// Libera al objetivo (punto 6): quita la habilidad de parálisis
	// concedida por BeginEmbeddedEffect. Debe llamarse siempre al
	// recuperar el arma mientras siga clavada en un actor (ver
	// WeaponManager::RecallWeapon).
	void EndEmbeddedEffect(RE::Actor* a_target);
}
