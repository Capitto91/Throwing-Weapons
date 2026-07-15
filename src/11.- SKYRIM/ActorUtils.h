// Contiene funciones auxiliares relacionadas con actores de Skyrim.
// Facilita comprobaciones de enemigos, distancias y selección de objetivos.

#pragma once

namespace ActorUtils
{
	// Devuelve true si el actor tiene equipada en la mano derecha (mano
	// principal) el arma arrojadiza única (identificada por su Keyword, ver
	// Constants::kThrowableWeaponKeyword). La mano izquierda no participa en
	// esta mecánica.
	bool IsThrowableWeaponEquipped(RE::Actor* a_actor);

	// Punto 6 de Mecanica del arma.txt: busca el nodo más cercano a
	// a_worldPoint dentro del árbol 3D de a_actor (recorrido recursivo de
	// NiNode::GetChildren(), sin filtrar por tipo de nodo -- el más cercano
	// a un punto de la superficie del cuerpo suele ser un hueso real). Sin
	// esto, el clavado en un actor (Combat::BeginEmbeddedEffect) solo podía
	// aproximarse desde el nodo raíz, sin seguir el movimiento de huesos
	// individuales (comprobado en el juego: se veía flotando). Devuelve un
	// BSFixedString vacío si el actor no tiene 3D.
	RE::BSFixedString FindNearestBoneName(RE::Actor* a_actor, const RE::NiPoint3& a_worldPoint);
}
