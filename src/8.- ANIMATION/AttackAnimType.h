// Ataques de maza con el arma arrojadiza, conservando la cadera y el
// desenvainado/envainado de hacha (2026-09-28).
//
// El registro WEAP es OneHandAxe. Vanilla no distingue ataques de hacha y
// de maza a una mano (mismos clips 1hm_attack*); quien los distingue son
// los packs de animación, vía la condición IsEquippedType de OAR, que lee
// TESObjectWEAP::GetWeaponType() (= weaponData.animationType) del arma
// equipada en cada evaluación, sin caché (verificado en el código fuente
// de OAR, Conditions.cpp, IsEquippedTypeCondition::GetEquippedType).
//
// Por eso se cambia ese campo del formulario en tiempo de ejecución:
// Constants::kDrawnAnimationWeaponType mientras el arma está desenvainada
// del todo (ActorState::WEAPON_STATE::kDrawn), el tipo original del
// registro en cualquier otro estado (envainada, desenvainando,
// envainando) o al desequiparla. El cambio no se guarda en la partida (es
// dato del formulario, no de la referencia) y el registro vuelve a su tipo
// original en cada carga.
//
// Señal: un sink de BSAnimationGraphEvent sobre los grafos del jugador
// (tercera y primera persona) que reevalúa el estado con cada evento. El
// cambio se escribe en el propio hilo de animación que notifica, sin
// AddTask, para llegar antes que la siguiente evaluación de OAR (que
// también corre en ese hilo). Carrera conocida, sin verificar todavía en
// el juego: el primer evento que se recibe al empezar a envainar puede
// llegar después de que OAR ya haya elegido el clip de envainado, que
// entonces sería el de maza.
//
// Solo el jugador: un NPC con el arma no registra sink. Como el tipo es
// del formulario, un NPC que la lleve equipada mientras el jugador la
// tiene desenvainada vería también el tipo de maza.

#pragma once

namespace Animation::AttackAnimType
{
	// Registra el sink en todos los grafos de animación de a_actor
	// (idempotente) y sincroniza el tipo con el estado actual. Llamar al
	// equipar el arma, tras cargar partida y al cerrar una pantalla de
	// carga (el grafo puede haberse recreado).
	void EnsureRegistered(RE::Actor& a_actor);

	// Devuelve a_weapon a su tipo original si se había cambiado. Llamar al
	// desequiparla, para que no quede como maza al volver a equiparla
	// envainada (la colgaría del nodo de cadera de maza).
	void Restore(RE::TESObjectWEAP* a_weapon);
}
