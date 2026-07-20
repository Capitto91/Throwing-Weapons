// Controla las animaciones especiales del arma.
// Gestiona giro durante vuelo, orientación antes del impacto y animación de agarre.

#pragma once

// Punto 10 de Mecanica del arma.txt: giro sobre sí misma durante el vuelo.
// Calculado y escrito directamente por código cada tick sobre el nodo hijo
// dedicado (Constants::kWeaponSpinNodeName), sin ninguna animación
// horneada en el NIF ni NiTimeController/NiInterpolator de por medio.
// Motivo (ver CHANGELOG.md para el historial completo): se probaron tres
// arquitecturas basadas en NiTimeController (controller suelto,
// NiControllerManager/NiControllerSequence, y una variante horneada activa
// desde la carga sin ningún Start() por código) y las tres fallaban de
// forma intermitente porque el motor no siempre llega a llamar
// NiTimeController::Update() por su cuenta sobre el controller de una
// réplica creada en tiempo de ejecución; y llamar a Update() a mano desde
// fuera del propio recorrido del motor crasheó el juego. Escribir
// directamente NiAVObject::local.rotate es el mismo tipo de operación que
// SetPosition/SetAngle, ya usada sin problemas en todo el proyecto -- no
// depende de ningún método interno del motor, así que el mismo bucle de
// tick que ya mueve la réplica de forma fiable (ver 4.- THROW/ThrowManager
// y 5.- RETURN/ReturnManager) es suficiente, sin ningún hook.

namespace Animation
{
	// Avanza a mano el giro de a_refr: calcula un ángulo a partir de
	// a_elapsedSeconds (Constants::kSpinAngularSpeed) y lo escribe en la
	// rotación local del nodo Constants::kWeaponSpinNodeName -- la
	// posición/escala del nodo no se tocan. Debe llamarse cada tick del
	// mismo bucle de movimiento manual que ya mueve la réplica, antes de
	// Physics::SyncHavok (que ya recalcula el árbol de transformaciones
	// mundiales, incluida la de este nodo hijo). Sin efecto si la réplica
	// no tiene el nodo todavía (NIF sin cargar del todo) -- en ese caso el
	// giro empieza a verse en cuanto el nodo aparezca, sin reintentos
	// explícitos.
	void TickSpin(RE::TESObjectREFR& a_refr, float a_elapsedSeconds);

	// Punto 11: temblor de desprendimiento antes de iniciar el regreso
	// desde un objetivo clavado (Constants::kStickShudderDuration,
	// llamado desde 5.- RETURN/ReturnManager::BeginReturn antes de
	// arrancar el movimiento de vuelta en sí, nunca durante el vuelo).
	// Escribe una oscilación de frecuencia creciente (chirp de fase
	// continua, ver Constants::kStickShudderFrequencyStart/End) y amplitud
	// creciente (crecimiento exponencial hacia Constants::kStickShudderMaxAngle,
	// ver Constants::kStickShudderAmplitudeRampFraction) sobre el mismo
	// nodo de giro visual que TickSpin (Constants::kWeaponSpinNodeName) --
	// puramente visual, no toca el ángulo lógico de a_refr ni Havok.
	//
	// a_baseRotation es la rotación que tenía el nodo de giro en el
	// instante de quedar clavada (capturada una única vez por el llamante
	// antes de empezar el temblor, ver Return::BeginReturn) -- la
	// oscilación se compone *sobre* ella (a_baseRotation * oscilación) en
	// vez de sustituirla, para que a_elapsedSeconds=0 deje el arma
	// exactamente en la misma orientación en la que se clavó, sin ningún
	// salto visual (a diferencia de TickSpin, que sí escribe una rotación
	// absoluta desde cero: aquí el arma ya viene de un ángulo de vuelo
	// arbitrario, no del reposo).
	//
	// a_elapsedSeconds debe ir de 0 (reposo, sin oscilación) a
	// Constants::kStickShudderDuration. Mismo comportamiento que TickSpin
	// si el nodo de giro no existe todavía (sin efecto, sin reintento
	// explícito).
	void TickShudder(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_baseRotation, float a_elapsedSeconds);

	// Transformación mundial a usar como base para efectos que deben
	// acompañar el giro visual (p. ej. la estela, ver
	// 8.- ANIMATION/WeaponTrail): el nodo raíz de la réplica (a_root) no
	// gira durante el vuelo -- SetAngle nunca cambia, solo se mueve la
	// posición -- así que su world transform por sí sola no refleja el
	// giro, que vive solo en el nodo hijo Constants::kWeaponSpinNodeName.
	// Devuelve la transformación de ese nodo si existe (misma posición
	// que el raíz, rotación actualizada por TickSpin cada tick), o la del
	// raíz tal cual si el nodo de giro no existe todavía.
	RE::NiTransform GetVisualTransform(RE::NiAVObject& a_root);
}
