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

#pragma once

#include "8.- ANIMATION/WeaponTrail.h"

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
		// cada una.
		void Start(RE::TESObjectCELL* a_cell, const RE::NiPoint3& a_initialPosition, const RE::NiPoint3& a_upReference, float a_roll, const RE::NiPoint3& a_anchorWorldOffset);

		// Mismo criterio que Start: a_roll es la base de la copia 0, cada
		// copia mantiene su propio desfase fijo por encima.
		void SetRoll(float a_roll);

		// Reenvía a_currentPosition/a_deltaSeconds a todas las copias por
		// igual -- todas siguen exactamente la misma posición histórica,
		// solo difieren en su roll.
		void Update(const RE::NiPoint3& a_currentPosition, float a_deltaSeconds);

	private:
		std::vector<WeaponTrail> trails;
	};
}
