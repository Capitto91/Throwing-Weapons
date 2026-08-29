// Destello al clavarse el arma (punto 6/9 de Mecanica del arma.txt, aunque
// el VFX en sí es puro polish sin número propio -- mismo criterio que
// 8.- ANIMATION/WeaponVFX.h/WeaponGlow.h).
//
// Pivote 2026-08-29: descartado por completo el .nif propio
// (ThorMjolnirImpact.nif, ver el histórico largo en CHANGELOG.md/
// NIF-PARAMETERS.md -- varias rondas de crashes de carga nativa nunca
// pinpointeados del todo) a favor de una explosión 100% vanilla real
// (Constants::kImpactExplosionLocalFormID, BGSExplosion de Skyrim.esm) --
// sin ningún .nif propio, sin NifSkope, sin controladores/secuencias
// hechos a mano, sin pulso de escala por código. Un BGSExplosion es un
// TESBoundObject como cualquier Activator (ver RE/B/BGSExplosion.h), así
// que se coloca con el mismo RE::TESObjectREFR::PlaceObjectAtMe ya usado
// en todo el proyecto -- el motor se encarga solo de todo lo demás
// (partículas, luz, sonido, radio, autodestrucción -- Explosion::age/
// lifetime en RE/E/Explosion.h), sin ningún hilo ni temporizador propio.

#pragma once

namespace Animation
{
	// Coloca Constants::kImpactExplosionLocalFormID en a_position (mundo,
	// fija) y deja que el motor la reproduzca y limpie sola -- fire-and-
	// forget, sin Stop()/handle expuesto, cada llamada es independiente.
	//
	// a_spawnAt es la referencia sobre la que se llama PlaceObjectAtMe --
	// solo para heredar su celda/worldspace correctos (mismo criterio que
	// Animation::WeaponVFX::StartOn), la posición real la da a_position.
	// Sin efecto, con aviso en el log, si
	// Constants::kImpactExplosionLocalFormID no resuelve a un BGSExplosion
	// real.
	void SpawnImpactVFX(RE::TESObjectREFR& a_spawnAt, const RE::NiPoint3& a_position);
}
