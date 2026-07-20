// Define el cálculo matemático de la trayectoria de retorno.
// Permite crear curvas, desviaciones y búsqueda de objetivos durante el regreso.

#pragma once

// Cálculos propios del regreso (puntos 7-8 de Mecanica del arma.txt):
// punto de control de la curva de Bezier cuadrática y aceleración
// híbrida. La evaluación genérica de la curva en sí vive en
// 9.- MATH/CurveMath (reutilizable fuera del contexto del regreso).

namespace Return
{
	// Vector "derecha" del actor en este instante (eje X de su nodo raíz).
	// Se usa para decidir hacia qué lado se desvía la curva del regreso:
	// el arma se recoge con la mano derecha, así que debe entrar por ese
	// lado (ver ComputeReturnControlPoint). Vector mundial {1,0,0} si el
	// actor no tiene 3D cargado.
	RE::NiPoint3 GetPlayerRightVector(RE::Actor* a_actor);

	// Punto de control de la curva de Bezier cuadrática entre a_start y
	// a_end (punto 7: nunca en línea recta). Se ancla a lo largo de la
	// línea recta inicio→fin en a_anchorFraction (0 = en a_start, 1 = en
	// a_end; 0.5 = punto medio) y desde ahí se desvía perpendicularmente
	// a la línea recta hacia el lado de a_rightVector (proyectado
	// perpendicular a esa línea vía Gram-Schmidt; si resulta casi
	// paralelo —caso degenerado—, se cae a perpendicular sobre el eje Z
	// del mundo), una fracción de la distancia total
	// (Constants::kReturnCurveLateralFraction, acotada entre
	// kReturnCurveMinOffset/kReturnCurveMaxOffset). a_anchorFraction es un
	// concepto distinto de esa fracción lateral: una controla *dónde a lo
	// largo de la línea* se ancla el punto de control, la otra *cuánto* se
	// desvía lateralmente desde ahí -- no confundirlas.
	RE::NiPoint3 ComputeReturnControlPoint(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, const RE::NiPoint3& a_rightVector, float a_anchorFraction);

	// Aceleración híbrida (punto 8): Constants::kReturnAcceleration por
	// defecto, salvo que a esa aceleración se tardara más de
	// Constants::kReturnMaxDuration en cubrir a_distance partiendo del
	// reposo — en ese caso se calcula la aceleración mínima necesaria
	// para cumplir ese límite (d = ½·a·t² despejando a).
	float ComputeReturnAcceleration(float a_distance);

	// Distancia recorrida tras a_elapsedSeconds acelerando desde el
	// reposo a a_acceleration constante (½·a·t²). Se usa para saber en
	// qué punto (0-1) de la curva está el arma cada tick, dividiendo
	// entre la distancia recta inicial capturada al empezar el regreso.
	float ComputeTraveledDistance(float a_acceleration, float a_elapsedSeconds);
}
