// Utilidades sobre actores: arma arrojadiza equipada, hueso más cercano a un punto y cámara del jugador.

#pragma once

namespace ActorUtils
{
	// true si a_actor lleva en la mano derecha un arma con Constants::kThrowableWeaponKeyword.
	bool IsThrowableWeaponEquipped(RE::Actor* a_actor);

	// Nombre del nodo del 3D de a_actor más cercano a a_worldPoint (vacío sin 3D).
	// Lo usa Combat::BeginEmbeddedEffect para seguir el hueso donde se clava el arma.
	RE::BSFixedString FindNearestBoneName(RE::Actor* a_actor, const RE::NiPoint3& a_worldPoint);

	// true con la cámara en primera persona (IFPV la deja en tercera y cuenta como tal). WeaponManager y
	// LightningDash eligen con ello los tiempos de los clips de primera persona.
	bool IsPlayerInFirstPerson();
}
