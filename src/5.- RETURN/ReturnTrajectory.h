// Cálculo matemático de la trayectoria de retorno: curva de Bezier
// cuadrática (punto 7 de Mecanica del arma.txt) recorrida con aceleración
// híbrida partiendo de velocidad cero (punto 8). Pendiente: enderezado
// antes de llegar (punto 11).

#pragma once

namespace Return
{
	// Punto de control de la curva de Bezier cuadrática usada para el
	// regreso (punto 7: nunca en línea recta). Se calcula una única vez,
	// al empezar el regreso (ver Return::BeginReturn/StartControlling), a
	// partir de a_start (posición de partida) y a_end (mano del jugador
	// en ese instante) — no se recalcula aunque la mano se mueva después,
	// solo el punto final de la curva sigue a la mano en cada tick.
	// a_preferredSide indica hacia qué lado curvar (se usa el vector
	// "derecha" del jugador en el instante de empezar el regreso, para
	// que el arma entre por el lado de la mano derecha, donde se recoge,
	// en vez de un lado arbitrario); si es paralelo a la línea recta
	// start-end (caso degenerado), se cae a una perpendicular horizontal
	// cualquiera.
	[[nodiscard]] RE::NiPoint3 ComputeReturnControlPoint(const RE::NiPoint3& a_start, const RE::NiPoint3& a_end, const RE::NiPoint3& a_preferredSide);

	// Distancia recorrida tras a_elapsedTime segundos de aceleración
	// constante a_acceleration partiendo de velocidad cero (misma
	// cinemática que ComputeReturnAcceleration: distancia = ½·a·t²). Se
	// usa cada tick para saber en qué punto (0-1) de la curva de Bezier
	// está el arma.
	[[nodiscard]] float ComputeTraveledDistance(float a_acceleration, float a_elapsedTime);

	// Aceleración a aplicar durante todo el regreso (punto 8 de Mecanica
	// del arma.txt, partiendo siempre de velocidad cero): a_distance es la
	// distancia total a recorrer, capturada al empezar el regreso. Se usa
	// a_defaultAcceleration salvo que, partiendo del reposo a esa
	// aceleración, recorrer a_distance tardara más de a_maxDuration
	// segundos — en ese caso devuelve la aceleración mínima necesaria para
	// completarlo en exactamente a_maxDuration (distancia = ½·a·t²).
	[[nodiscard]] float ComputeReturnAcceleration(float a_distance, float a_defaultAcceleration, float a_maxDuration);
}
