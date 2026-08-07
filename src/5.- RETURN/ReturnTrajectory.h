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

	// Coeficiente de aceleración híbrido (punto 8, cambio de criterio --
	// ver CLAUDE.md/Constants::kReturnAccelerationExponent): sigue
	// llamándose "aceleración" por continuidad con el significado físico
	// del valor, pero desde que la rampa dejó de ser una recta (aceleración
	// constante) para ser una curva de grado
	// Constants::kReturnAccelerationExponent, este coeficiente ya no es
	// literalmente la aceleración en todo instante -- ver
	// ComputeTraveledDistance para la fórmula completa.
	//
	// Cambio de criterio (2026-08-07, ver CLAUDE.md/
	// Constants::kReturnTargetArrivalSpeed): ya no es una constante fija --
	// se recalcula por a_distance para que la velocidad justo al llegar a
	// la mano sea siempre Constants::kReturnTargetArrivalSpeed, sin
	// importar la distancia recorrida (con una aceleración fija, un
	// regreso corto se queda todo el trayecto dentro del primer tramo de
	// la rampa sin llegar a coger velocidad -- se veía desproporcionadamente
	// lento frente a uno largo, y subir el coeficiente fijo no lo
	// corregía, solo escalaba la lentitud por igual en todas las
	// distancias). Salvo que a ese ritmo se tardara más de
	// Constants::kReturnMaxDuration en cubrir a_distance -- en ese caso se
	// calcula el mínimo necesario para cumplir ese límite en su lugar,
	// sacrificando la velocidad de llegada constante a cambio de no superar
	// la duración máxima.
	float ComputeReturnAcceleration(float a_distance);

	// Distancia recorrida tras a_elapsedSeconds con el perfil de
	// aceleración creciente del punto 8 (cambio de criterio, ver
	// Constants::kReturnAccelerationExponent): d(t) = a_acceleration /
	// (n·(n-1)) · t^n, con n = Constants::kReturnAccelerationExponent --
	// forma cerrada (no acumulada tick a tick, mismo criterio que
	// Throw::ComputeGravityDrop), verificada por doble derivación: con
	// n=2 colapsa exactamente en la fórmula anterior de aceleración
	// constante (½·a·t²). Se usa para saber en qué punto (0-1) de la
	// curva está el arma cada tick, dividiendo entre la distancia recta
	// inicial capturada al empezar el regreso.
	float ComputeTraveledDistance(float a_acceleration, float a_elapsedSeconds);

	// Inversa de ComputeTraveledDistance: duración prevista (segundos)
	// hasta que el arma recorra a_distance con el perfil de aceleración
	// a_acceleration -- T = (a_distance·n·(n-1) / a_acceleration)^(1/n).
	// Válida tanto si a_acceleration viene del régimen por defecto de
	// ComputeReturnAcceleration (velocidad de llegada constante) como del
	// recalculado para cumplir kReturnMaxDuration (en ese caso el resultado
	// es exactamente kReturnMaxDuration, por construcción del propio
	// recálculo -- verificado algebraicamente, no solo probado). Usada
	// para predecir con antelación el instante de llegada y poder
	// adelantar sonidos/efectos que necesiten cuadrar con ese instante
	// exacto (ver Constants::kCatchStartSoundLeadTime, Return::BeginReturn)
	// en vez de solo detectar la llegada tick a tick.
	float ComputeReturnDuration(float a_acceleration, float a_distance);

	// Inversa de ComputeReturnDuration pero despejando la aceleración a
	// partir de una duración objetivo cualquiera (a_targetDuration), no de
	// un coeficiente ya dado: a = d·n·(n-1) / T^n -- mismo despeje que ya
	// usa ComputeReturnAcceleration internamente para su límite superior
	// (Constants::kReturnMaxDuration), aquí expuesto para el caso
	// contrario, un límite *inferior*.
	//
	// Reintroducida 2026-08-07 (existió antes, se eliminó por quedar sin
	// uso al pasar la sincronización animación/física a apoyarse en el
	// temblor de desprendimiento en vez de en el propio vuelo, ver
	// CLAUDE.md) para un caso concreto que el temblor no puede cubrir:
	// Return::BeginReturnMovement, solo cuando el regreso arranca sin haber
	// pasado por el temblor (a_wasStuck=false en Return::BeginReturn, p.
	// ej. recuperar el arma en pleno vuelo de ida antes de que impacte) --
	// ahí no hay ningún temblor que alargar para darle tiempo real a la
	// animación de Atrape a sincronizarse (Constants::kMinCatchAnimationDelay/
	// kCatchAnimationLeadTime), así que si la aceleración de llegada
	// constante (ComputeReturnAcceleration) terminaría el vuelo antes de
	// ese margen, se ralentiza *solo ese caso* con esta función -- nunca
	// el caso normal (arma clavada, mucho más frecuente), que sigue
	// siempre a velocidad natural. a_distance/a_targetDuration deben ser
	// mayores que cero (sin comprobación defensiva aquí -- el llamante ya
	// lo garantiza, mismo criterio que el resto de este módulo).
	float ComputeReturnAccelerationForDuration(float a_distance, float a_targetDuration);
}
