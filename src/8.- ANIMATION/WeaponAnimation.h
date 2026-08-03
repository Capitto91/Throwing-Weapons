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
	// a_elapsedSeconds y lo escribe en la rotación local del nodo
	// Constants::kWeaponSpinNodeName -- la posición/escala del nodo no se
	// tocan. La velocidad angular ya no es constante desde el instante
	// cero (a petición del usuario): sube en línea recta desde 0 hasta
	// Constants::kSpinAngularSpeed a lo largo de Constants::kSpinRampDuration
	// (aceleración angular constante durante la rampa, forma cerrada de
	// dos tramos empalmados en ángulo y velocidad angular -- mismo
	// criterio que Throw::ComputeGravityDrop, un orden de derivada más
	// abajo), después constante como antes. Debe llamarse cada tick del
	// mismo bucle de movimiento manual que ya mueve la réplica, antes de
	// Physics::SyncHavok (que ya recalcula el árbol de transformaciones
	// mundiales, incluida la de este nodo hijo). Sin efecto si la réplica
	// no tiene el nodo todavía (NIF sin cargar del todo) -- en ese caso el
	// giro empieza a verse en cuanto el nodo aparezca, sin reintentos
	// explícitos.
	void TickSpin(RE::TESObjectREFR& a_refr, float a_elapsedSeconds);

	// Punto 10 (segunda mitad, giro): "justo antes de... volver a la mano
	// del jugador, se endereza para que... el mango quede orientado para
	// que el jugador pueda agarrarla" -- sustituye a TickSpin durante la
	// ventana de enderezado (Constants::kSpinStraightenDuration, llamada
	// desde Return::BeginReturnMovement en los últimos instantes del
	// regreso, arrancada junto con Return::ReturnCallbacks::onApproaching).
	// Recalcula internamente (misma fórmula que TickSpin) el ángulo que
	// tenía el giro justo al empezar la ventana a partir de
	// a_elapsedAtWindowStart, y lo interpola (curva suave, no lineal)
	// hacia 0 -- la orientación de reposo del nodo, la misma que ya tiene
	// el arma real equipada (nunca lleva rotación extra sobre este nodo)
	// -- según a_blend avanza de 0 (recién empezada la ventana, ángulo
	// intacto) a 1 (terminada, coincidiendo con la orientación de agarre).
	// a_blend fuera de [0,1] se acota. Mismo comportamiento que TickSpin
	// si el nodo de giro no existe todavía.
	void TickSpinStraighten(RE::TESObjectREFR& a_refr, float a_elapsedAtWindowStart, float a_blend);

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

	// Fase 3 del plan OAR (_reference/PLAN-OAR.md): activa/desactiva
	// Constants::kThrowTriggerGraphVariable en a_actor. Envuelta aquí (en vez
	// de llamar SetGraphVariableBool directo desde WeaponManager) para que
	// el nombre de la variable viva en un solo sitio, mismo motivo que
	// TickSpin/TickShudder.
	void SetThrowTrigger(RE::Actor& a_actor, bool a_active);

	// Mismo patrón que SetThrowTrigger, para Llamada: activa/desactiva
	// Constants::kCallTriggerGraphVariable en a_actor.
	void SetCallTrigger(RE::Actor& a_actor, bool a_active);

	// Mismo patrón que SetThrowTrigger/SetCallTrigger, para Atrape: activa/
	// desactiva Constants::kCatchTriggerGraphVariable en a_actor.
	void SetCatchTrigger(RE::Actor& a_actor, bool a_active);

	// Prueba (ver _reference/PLAN-OAR.md, 2026-07-29): activa/desactiva
	// Constants::kAnimationDrivenGraphVariable, una graph variable vanilla
	// (no propia), junto con SetThrowTrigger durante State::kThrowing --
	// a ver si evita la escalada a power attack direccional cuando el
	// jugador ya llevaba movimiento al soltar el botón.
	void SetAnimationDriven(RE::Actor& a_actor, bool a_active);

	// Oculta (o vuelve a mostrar) el nodo visual "WEAPON" del arma
	// actualmente equipada en a_actor -- mismo nodo ya usado como origen del
	// lanzamiento (4.- THROW/ThrowManager) y orientación de llegada del
	// regreso (5.- RETURN/ReturnManager). Usado por
	// WeaponManager::ThrowWeapon para que el arma real desaparezca en el
	// instante exacto de la liberación sin desequiparla todavía (ver
	// Constants::kThrowReleaseVisualHoldDuration). Devuelve false (sin
	// tocar nada) si el nodo no existe o no tiene hijos todavía -- caso real
	// tras un EquipObject muy reciente (ver
	// WeaponManager::PollGestureWeaponReady), no solo "arma sin 3D cargado"
	// en general.
	bool SetEquippedWeaponHidden(RE::Actor& a_actor, bool a_hidden);
}
