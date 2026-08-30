// Destello de un solo uso sobre las dos manos del jugador, ver
// Constants.h ("-- Brillo de manos --") para la arquitectura completa
// (BGSArtObject vía ApplyArtObject, por qué frente a un TESEffectShader,
// y la advertencia sin verificar sobre selección de NiControllerSequence).

#pragma once

namespace RE
{
	class Actor;
}

namespace Animation
{
	// Aplica Constants::kHandGlowArtObjectLocalFormID sobre el hueso de
	// cada mano de a_actor (Constants::kHandGlowLeftHandNodeName/
	// kHandGlowRightHandNodeName) -- destello de un solo uso, gestionado
	// enteramente por el motor (RE::ModelReferenceEffect se retira solo
	// pasado Constants::kHandGlowDuration, no hace falta guardar ningún
	// puntero ni desactivarlo a mano, a diferencia de Animation::WeaponGlow).
	//
	// Pensada para llamarse una vez por cada instante de lanzar/recibir
	// el arma (WeaponManager::BeginThrowAnimation/BeginCatchAnimation) --
	// no es un estado persistente, así que no hace falta "desactivarlo"
	// en ningún otro punto del ciclo.
	//
	// Sin efecto (con aviso en el log) si el BGSArtObject no resuelve.
	// Si Actor::GetNodeByName no encuentra el hueso de una mano concreta,
	// esa mano en particular se salta (con aviso), la otra se aplica
	// igual -- nunca las dos a la vez sin ningún intento.
	//
	// Debe llamarse desde el hilo principal (dentro de una tarea de
	// SKSE::GetTaskInterface()->AddTask), igual que cualquier otra
	// función que toque 3D/formularios del actor.
	void TriggerHandGlow(RE::Actor& a_actor);
}
