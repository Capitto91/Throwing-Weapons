// Varias copias de Animation::WeaponTrail en paralelo, cada una con su
// propio ángulo de roll fijo (Constants::kTrailCopyRollStepDegrees entre
// copias consecutivas).
//
// Sustituye (2026-08-26) al intento de una única malla con varios planos
// cruzados dentro del mismo WeaponTrail: con los pesos de piel repartidos
// entre huesos vecinos (necesario para que la cinta no se vea a bloques
// al curvar, ver WeaponTrail.cpp), las aspas más alejadas del eje de los
// huesos se aplastaban cuando dos huesos consecutivos recibían
// orientaciones bastante distintas (el arreglo de "poca resolución",
// tangente por segmento) -- el promedio ponderado que hace el motor entre
// las dos transformaciones de hueso, aplicado a un vértice lejos del eje,
// lo arrastra hacia el centro. Con el plano estrecho original esto apenas
// se notaba (ningún vértice lejos del eje); con varias aspas anchas en la
// misma malla, sí. Reportado por el usuario en el juego, contrastado
// contra el preview de CK (sin este problema, ahí los huesos no reciben
// ninguna orientación de código, se quedan en su pose de fábrica).
//
// En vez de perseguir un reparto de pesos que evite el problema, varias
// copias INDEPENDIENTES del mismo .nif de un único plano (ya probado,
// sin este riesgo) giradas entre sí dan la misma cobertura visual desde
// cualquier ángulo sin tocar NifSkope.
//
// Misma interfaz pública que WeaponTrail (Start/Update/SetRoll) para que
// los llamantes (Throw::LaunchWeapon/Return::BeginReturnMovement) no
// necesiten ningún cambio más que el tipo.
//
// Efecto rayo (2026-08-26, a petición del usuario -- "pequeñas
// desviaciones de manera aleatoria, sin salirse demasiado de la línea de
// trayectoria"): Update() desvía la posición real un desplazamiento
// aleatorio acotado (Constants::kTrailLightningMaxDeviation), perpendicular
// a la dirección de avance real (estimada aquí mismo por diferencia con la
// posición del tick anterior, no necesita que el llamante pase nada
// nuevo). La base perpendicular se calcula una única vez por tick a partir
// de la trayectoria real sin desviar, pero el sorteo del desvío en sí es
// INDEPENDIENTE por copia (segunda vuelta, mismo día: compartir un único
// desvío entre las 8 se notaba como que "todas se desvían de la misma
// manera") -- cada una de las 8 traza su propio zigzag, todas centradas en
// la misma trayectoria real de fondo.
//
// 2026-08-27, primer paso hacia un aspecto de rayo más brusco (ver
// Constants::kTrailLightningHoldSeconds): el desvío de cada copia ya NO se
// resortea todos los ticks -- se mantiene fijo un rato (heldDeviation*/
// holdTimer, uno por copia) y solo se re-sortea al agotarse ese tiempo.
// Sigue proyectándose sobre la base right/up del tick actual (que sí se
// recalcula cada tick a partir de la trayectoria real), así que el vector
// resultante todavía sigue la dirección de avance -- solo su MAGNITUD por
// eje se mantiene constante entre resorteos.

#pragma once

#include "8.- ANIMATION/WeaponTrail.h"

#include <optional>
#include <random>
#include <vector>

namespace Animation
{
	class WeaponTrailGroup
	{
	public:
		// Reserva Constants::kTrailCopyCount instancias de WeaponTrail,
		// todavía sin arrancar (Start() las arranca a todas).
		WeaponTrailGroup();

		// Mismos parámetros que WeaponTrail::Start -- a_roll es el roll de
		// la copia 0; el resto suman i·Constants::kTrailCopyRollStepDegrees
		// cada una. Reinicia también el seguimiento de posición previa del
		// efecto rayo (ver cabecera del archivo) -- un tramo nuevo no debe
		// heredar la dirección de avance del tramo anterior.
		void Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset);

		// Mismo criterio que Start: a_roll es la base de la copia 0, cada
		// copia mantiene su propio desfase fijo por encima.
		void SetRoll(float a_roll);

		// Desvía a_currentPosition con un desplazamiento propio para cada
		// copia (ver cabecera del archivo) y reenvía el resultado, junto
		// con a_deltaSeconds, a cada una.
		void Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds);

	private:
		std::vector<WeaponTrail>    trails;
		std::mt19937                randomEngine{ std::random_device{}() };
		std::optional<RE::NiPoint3> previousRawPosition;

		// Desvío mantenido (escalares sobre right/up del tick actual, ver
		// cabecera del archivo) y tiempo transcurrido desde el último
		// resorteo, uno por copia -- índices paralelos a trails.
		std::vector<float> heldDeviationRight;
		std::vector<float> heldDeviationUp;
		std::vector<float> holdTimers;
	};
}
