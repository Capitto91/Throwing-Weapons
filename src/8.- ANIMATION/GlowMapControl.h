// Glow de la textura del propio martillo (glow map de Mjolnir.nif), con
// modo configurable (2026-09-28, a petición del usuario): apagado,
// constante o en pulso, y siempre o solo con dragones / no muertos /
// daedra vivos cerca (ver Settings::GlowMode/GlowCondition). Puro polish,
// sin punto numerado en Mecanica del arma.txt.
//
// Mecanismo: cada tick se escribe BSLightingShaderProperty::emissiveMult de
// las mallas cuyo material es un glow map (BSShaderMaterial::Feature::
// kGlowMap), escalando el valor original del .nif (guardado la primera vez
// que se lee cada malla, por nombre). Mismo criterio que el resto de VFX
// del proyecto: animación por código, nada horneado en el NIF (ver la
// skill nif-vfx-practices). Sin verificar todavía en el juego que el
// motor respete el valor escrito en un arma equipada.
//
// Se aplica al arma arrojadiza equipada por el jugador (desenvainada o
// envainada, en primera y tercera persona -- localizada por los datos de
// biped del actor, BIPOBJECT::partClone) y a la réplica del ciclo
// (lanzada, clavada o regresando). No toca el arma en el suelo, en otros contenedores ni en
// manos de un NPC.
//
// Un único bucle de tick permanente (Physics::StartTickLoop sobre el
// jugador); la búsqueda de criaturas cercanas se hace cada
// Constants::kGlowMapCreatureScanIntervalSeconds, no cada tick.

#pragma once

namespace Animation::GlowMapControl
{
	// Arranca el bucle si no está ya en marcha (idempotente). Seguro desde
	// cualquier hilo y desde dentro de una tarea: el arranque real se
	// difiere al hilo principal. Llamar al cargar/empezar partida, al
	// equipar el arma y al cerrar una pantalla de carga.
	void EnsureRunning();
}
