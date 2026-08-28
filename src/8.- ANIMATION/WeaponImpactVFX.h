// Destello al clavarse el arma (punto 6/9 de Mecanica del arma.txt, aunque
// el VFX en sí es puro polish sin número propio -- mismo criterio que
// 8.- ANIMATION/WeaponVFX.h/WeaponGlow.h).
//
// Arquitectura deliberadamente más simple que esos dos módulos hermanos:
// fire-and-forget puro, sin estado global. WeaponVFX/WeaponGlow existen
// alrededor de la invariante "solo hay un ciclo de arma a la vez" (un único
// VFX continuo activo, con su aparato de generación/reentrancia para
// relevarlo sin cortes) -- el VFX de impacto no comparte esa invariante:
// cada llamada coloca un Activator independiente
// (Constants::kImpactVfxActivatorLocalFormID, ThorMjolnirImpact.nif) en un
// punto FIJO del mundo, lo deja reproducirse solo y se autodestruye pasado
// Constants::kImpactVfxLifetime -- nunca hace falta seguir nada en
// movimiento ni pararlo desde fuera, así que no necesita ningún handle/
// token guardado a nivel de archivo (confirmado contra
// Physics::StartTickLoop, 6.- PHYSICS/PhysicsManager.cpp: el bucle interno
// se sostiene con su propio shared_ptr capturado por valor, el llamante
// solo necesita retener el TickToken si va a cancelarlo, y aquí nunca hace
// falta). Dos, tres o N impactos consecutivos (lanzamientos rápidos)
// coexisten sin ningún conflicto.
//
// El .nif (ThorMjolnirImpact.nif) lleva un NiBillboardNode "glow" ->
// "glow:0" (FXGlowSpotLinearAlpha.dds) cuyo pulso de "crece y luego mengua"
// (decodificado del NIF de referencia vanilla fxshockcloakhandeffects.nif,
// secuencias mCast/mCastCon) es 100% code-driven, mismo patrón que
// Animation::WeaponGlow::TickGlowFade/TickGlowPulse
// (NiAVObject::local.scale/world.scale + BSEffectShaderMaterial::baseColorScale).
//
// Sin partículas en v1 (decisión del usuario, 2026-08-28): el .nif también
// lleva dos NiParticleSystem ("PCloudPowerHand"/"PCloudPowerCore") y
// "lightRays01"/"FlameCloakMesh01", pero el birth rate de un
// NiParticleSystem no se puede animar por código (confirmado repetidas
// veces en este proyecto) -- solo funciona horneando una
// NiControllerSequence en el propio .nif, y montarla a mano en NifSkope
// (sin partir de una ya existente en un NIF de referencia, que arrastra
// enlaces a bloques que no existen en este archivo) resultó más
// complicado de lo que compensaba para esta primera versión. Este código
// nunca toca esos nodos -- da igual si el usuario los deja inertes en el
// .nif o los quita del todo.

#pragma once

namespace Animation
{
	// Coloca Constants::kImpactVfxActivatorLocalFormID en a_position (mundo,
	// fija -- nunca sigue nada en movimiento) y anima por código el pulso
	// de escala/brillo del nodo "glow" (Constants::kImpactGlowNodeName)
	// durante
	// Constants::kImpactPulseDurationSeconds -- autodestruido pasado
	// Constants::kImpactVfxLifetime. Fire-and-forget: sin Stop()/handle
	// expuesto, cada llamada crea una instancia totalmente independiente,
	// ver el comentario de arriba para el porqué.
	//
	// a_spawnAt es la referencia sobre la que se llama PlaceObjectAtMe --
	// solo para heredar su celda/worldspace correctos (mismo criterio que
	// Animation::WeaponVFX::StartOn), la posición real la da a_position.
	// Sin efecto, con aviso en el log, si
	// Constants::kImpactVfxActivatorLocalFormID no resuelve a un Activator
	// real.
	void SpawnImpactVFX(RE::TESObjectREFR& a_spawnAt, const RE::NiPoint3& a_position);
}
