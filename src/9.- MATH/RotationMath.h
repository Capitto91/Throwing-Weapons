// Biblioteca matemática para transiciones de rotación suaves.
// Usada para fundir la orientación real del arma (capturada del modelo
// equipado o de un hueso) con el giro calculado por código durante el
// vuelo -- ver 8.- ANIMATION/WeaponAnimation, punto 10 de
// "Mecanica del arma.txt".

#pragma once

namespace Math
{
	// Interpolación esférica (camino más corto) entre dos rotaciones,
	// expresadas como NiMatrix3 -- conversión a cuaternión y vuelta por
	// dentro (RE::NiQuaternion ya expone matriz<->cuaternión, ver
	// RE/N/NiQuaternion.h, pero no un Slerp propio). a_t fuera de [0,1] no
	// se acota -- responsabilidad del llamante (mismo criterio que
	// Math::EvaluateQuadraticBezier).
	RE::NiMatrix3 SlerpRotation(const RE::NiMatrix3& a_from, const RE::NiMatrix3& a_to, float a_t);

	// Rotación local que, compuesta con a_parentWorld (world = parent *
	// local -- convención ya usada en todo el proyecto, ver
	// 8.- ANIMATION/WeaponAnimation), reproduce exactamente a_desiredWorld.
	// a_parentWorld debe ser una rotación pura (ortonormal, su inversa es
	// su transpuesta) -- válido para cualquier NiMatrix3::world.rotate,
	// nunca para una matriz con escala.
	RE::NiMatrix3 LocalRotationFromWorld(const RE::NiMatrix3& a_parentWorld, const RE::NiMatrix3& a_desiredWorld);

	// Curva suave 0->1 (3t²-2t³), a_t fuera de [0,1] se acota -- extraída
	// aquí porque WeaponAnimation la necesita en varios sitios distintos
	// para el mismo tipo de transición de fundido (ver
	// Constants::kSpinRampDuration/kSpinStraightenLeadTime).
	float SmoothStep01(float a_t);

	// Construye a_matrix (rotación completa, 3 grados de libertad) a partir
	// de un eje de avance (a_forward, se coloca en el eje Y local -- mismo
	// convenio que usaba Precision) y un eje de referencia "hacia arriba"
	// (a_up) que fija el giro alrededor de ese eje de avance -- el grado de
	// libertad que un vector de avance por sí solo no puede fijar.
	// a_forward debe ser unitario; a_up no hace falta que sea ortogonal a
	// a_forward (se ortonormaliza con Gram-Schmidt), pero si es
	// (casi) paralelo a a_forward la referencia es degenerada y se cae a
	// un eje del mundo fijo distinto como respaldo.
	//
	// Sustituye a la SetRotationMatrix original de Precision (Ershin, MIT
	// License, github.com/ersh1/Precision, src/Utils.h,
	// Utils::SetRotationMatrix, que construía la rotación con solo 2
	// grados de libertad a partir de una única dirección -- Precision
	// fijaba el tercero con la propia rotación de la réplica, dato que este
	// proyecto no usa aquí, ver 8.- ANIMATION/WeaponTrail.cpp). Con solo 2
	// grados de libertad, el plano de la cinta salía orientado según una
	// referencia de mundo implícita sin relación con el ángulo real del
	// arma -- diagnosticado en el juego 2026-08-26 como la causa de que la
	// estela se viera "en un plano distinto" al del arma.
	//
	// a_roll (radianes) rota right/up alrededor de a_forward DESPUÉS de la
	// ortonormalización -- 2026-08-26, segunda vuelta: usar a_up = normal
	// real del plano de la trayectoria (sin bancado, ver
	// 8.- ANIMATION/WeaponTrail.cpp) ya no deja ningún grado de libertad
	// para encajar con el ángulo real del arma (esa normal es geometría
	// pura de la trayectoria, sin relación con cómo esté orientada el
	// arma). a_roll reintroduce ese grado de libertad sin reintroducir
	// bancado: el llamante lo calcula UNA vez, comparando en el instante
	// de lanzar la base sin bancado (a_up = normal de trayectoria) contra
	// la base que daría el eje real del arma -- y esa misma rotación fija
	// se aplica a todos los segmentos, así que nunca se acumula bancado
	// adicional, solo se parte de un ángulo de salida correcto.
	void SetRotationFromForwardUp(RE::NiMatrix3& a_matrix, const RE::NiPoint3& a_forward, const RE::NiPoint3& a_up, float a_roll);

	// Ángulo (radianes) que hay que pasar como a_roll a SetRotationFromForwardUp
	// para que, en a_forward, la base construida con a_planeUp (normal del
	// plano de trayectoria, sin bancado) coincida con la que daría
	// a_desiredUp -- calculado comparando el eje "right" de las dos bases
	// (SetRotationFromForwardUp con roll=0 en ambos casos) alrededor de
	// a_forward.
	//
	// Reintroducida 2026-08-26 (se había retirado en v1.14.38 por
	// resultados poco fiables usando el eje del arma como a_desiredUp --
	// diagnosticado después, en v1.14.39, que la causa real era un bug
	// estructural no relacionado, el reciclado de segmentos sin
	// sincronizar world; el cálculo del roll en sí no era el problema).
	// Uso actual, distinto del original: 8.- ANIMATION/WeaponTrail no
	// vuelve a usarla para el ángulo de salida (Constants::kTrailRollDegrees,
	// fijo), sino Return::BeginReturnMovement para fundir el roll de la
	// estela hacia la orientación real de la mano durante la misma
	// ventana de enderezado que ya usa Animation::TickSpinStraighten
	// (Constants::kSpinStraightenLeadTime) -- se recalcula cada tick
	// mientras dura esa ventana, no una vez fija.
	float ComputeRoll(const RE::NiPoint3& a_forward, const RE::NiPoint3& a_planeUp, const RE::NiPoint3& a_desiredUp);
}
