// Utilidades sobre actores: arma arrojadiza equipada, hueso del arma, equipado inmediato, hueso más cercano a un
// punto y cámara del jugador.

#pragma once

namespace ActorUtils
{
	// true si a_form es un arma con la keyword del arma arrojadiza (Forms::throwableWeaponKeyword).
	bool IsThrowableWeapon(const RE::TESForm* a_form);

	// true si a_actor lleva el arma arrojadiza en la mano derecha.
	bool IsThrowableWeaponEquipped(RE::Actor* a_actor);

	// Hueso del arma de la mano derecha (Constants::kWeaponNodeName), o nullptr sin 3D.
	[[nodiscard]] RE::NiAVObject* GetWeaponBone(RE::Actor& a_actor);

	// Posición mundial de ese hueso; sin él, la del actor. Origen del lanzamiento, destino del regreso y punto que
	// siguen las chispas.
	[[nodiscard]] RE::NiPoint3 GetWeaponBonePosition(RE::Actor& a_actor);

	// Equipa o desequipa a_object en el acto (sin cola y forzado). Desde un evento del motor, llamarlo diferido
	// un tick (AddTask): síncrono, a veces falla en silencio.
	void EquipNow(RE::Actor& a_actor, RE::TESBoundObject* a_object);
	void UnequipNow(RE::Actor& a_actor, RE::TESBoundObject* a_object);

	// Nombre del nodo del 3D de a_actor más cercano a a_worldPoint (vacío sin 3D).
	// Lo usa Combat::BeginEmbeddedEffect para seguir el hueso donde se clava el arma.
	RE::BSFixedString FindNearestBoneName(RE::Actor* a_actor, const RE::NiPoint3& a_worldPoint);

	// true con la cámara en primera persona (IFPV la deja en tercera y cuenta como tal). WeaponManager y
	// LightningDash eligen con ello los tiempos de los clips de primera persona.
	bool IsPlayerInFirstPerson();
}
