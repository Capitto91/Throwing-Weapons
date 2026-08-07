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
	// a_elapsedSeconds y lo compone SOBRE a_baseLocal -- la rotación LOCAL
	// real de la que partía el arma al empezar este tramo de vuelo (ver
	// GetEquippedWeaponWorldRotation/GetSpinLocalRotation + los llamantes,
	// Throw::LaunchWeapon/Return::BeginReturnMovement) -- no la sustituye.
	// La posición/escala del nodo no se tocan.
	//
	// a_baseLocal se mantiene como multiplicador constante durante TODO el
	// tramo, no solo en los primeros instantes: es el valor inicial de
	// lanzamiento el que marca la rotación de ahí en adelante, nunca al
	// revés (bug reportado por el usuario, 2026-08-06: con una versión
	// anterior de esta función que solo fundía hacia a_baseLocal durante
	// Constants::kSpinRampDuration y luego lo descartaba, el arma "salía
	// de la posición correcta pero se aplanaba momentos después" --
	// escribía la rotación calculada en bruto, anclada al nodo raíz, que
	// normalmente es una orientación casi horizontal, ver CLAUDE.md). La
	// velocidad angular en sí no es constante desde el instante cero (a
	// petición del usuario): sube en línea recta desde 0 hasta
	// Constants::kSpinAngularSpeed a lo largo de Constants::kSpinRampDuration
	// (aceleración angular constante durante la rampa, forma cerrada de
	// dos tramos empalmados en ángulo y velocidad angular -- mismo
	// criterio que Throw::ComputeGravityDrop, un orden de derivada más
	// abajo), después constante como antes -- eso sigue igual, es
	// ortogonal al problema de a_baseLocal de arriba.
	//
	// Debe llamarse cada tick del mismo bucle de movimiento manual que ya
	// mueve la réplica, antes de Physics::SyncHavok (que ya recalcula el
	// árbol de transformaciones mundiales, incluida la de este nodo
	// hijo). Sin efecto si la réplica no tiene el nodo todavía (NIF sin
	// cargar del todo) -- en ese caso el giro empieza a verse en cuanto el
	// nodo aparezca, sin reintentos explícitos.
	void TickSpin(RE::TESObjectREFR& a_refr, float a_elapsedSeconds, const RE::NiMatrix3& a_baseLocal);

	// Rotación LOCAL actual del nodo de giro (Constants::kWeaponSpinNodeName)
	// de a_refr -- identidad si el nodo no existe todavía. Pensado para
	// capturar "lo que llevara el giro en este instante" como a_baseLocal
	// de un TickSpin posterior (p. ej. al empezar el regreso tras un
	// temblor de desprendimiento), como a_blendFromLocal de un
	// TickSpinStraighten posterior (al detectarse un impacto), o como
	// a_currentLocal de ComputeImpactAlignment -- sin que el llamante
	// necesite conocer el nombre del nodo.
	RE::NiMatrix3 GetSpinLocalRotation(RE::TESObjectREFR& a_refr);

	// Punto 10 (segunda mitad, giro): "justo antes de... alcanzar un
	// objetivo o de volver a la mano del jugador, se endereza". Funde
	// (Math::SlerpRotation, curva suave) la rotación LOCAL del nodo de
	// giro desde a_blendFromLocal (capturada una única vez por el
	// llamante al empezar la ventana -- lo que llevara el giro en ese
	// instante, ver Return::BeginReturnMovement/Throw::LaunchWeapon) hacia
	// a_targetLocal, según a_blend avanza de 0 (recién empezada la
	// ventana) a 1 (terminada, coincidiendo exactamente con el objetivo).
	// a_targetLocal lo calcula el llamante -- puede recalcularse cada tick
	// (caso del regreso: la mano puede seguir girando mientras dura la
	// ventana, ver Math::LocalRotationFromWorld + GetHandBoneWorldRotation
	// más abajo) o mantenerse fijo (caso del impacto, ver
	// ComputeImpactAlignment: una vez clavada, nada vuelve a cambiar).
	// a_blend fuera de [0,1] se acota. Sin efecto si el nodo de giro no
	// existe todavía.
	void TickSpinStraighten(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_blendFromLocal, const RE::NiMatrix3& a_targetLocal, float a_blend);

	// Rotación LOCAL objetivo (respecto al nodo de giro) que alinea
	// Constants::kImpactAxisLocal con a_travelDirection en el instante del
	// impacto -- ver Math::ShortestArcRotation. a_currentLocal es la
	// rotación LOCAL que lleva el nodo de giro en este instante (ver
	// GetSpinLocalRotation) -- se parte de hacia dónde apunta
	// kImpactAxisLocal AHORA MISMO (a_rootWorld * a_currentLocal *
	// kImpactAxisLocal), no de una referencia de "reposo" asumida: el nodo
	// de giro puede tener su propia rotación local de partida distinta de
	// identidad (el offset que le diera NifSkope al montarlo, ver
	// CLAUDE.md), y desde el fix de TickSpin (2026-08-06) casi nunca está
	// en identidad durante el vuelo, así que asumir "reposo == identidad"
	// aquí habría medido la dirección equivocada -- posible causa real
	// (más allá de una posible corrección de signo pendiente en
	// Constants::kImpactAxisLocal) del bug reportado por el usuario de
	// quedar clavada por el mango en vez de por la cabeza. a_rootWorld es
	// la rotación mundial (constante durante toda la vida de la réplica)
	// del nodo raíz de a_refr, ver Math::LocalRotationFromWorld. Pensado
	// para usarse como a_targetLocal de TickSpinStraighten justo al
	// detectarse un impacto (punto 10, caso "objetivo": el filo/cabeza
	// debe apuntar hacia donde golpeó).
	RE::NiMatrix3 ComputeImpactAlignment(const RE::NiMatrix3& a_rootWorld, const RE::NiMatrix3& a_currentLocal, const RE::NiPoint3& a_travelDirection);

	// Rotación mundial actual de la malla real del arma equipada en
	// a_actor -- mismo nodo que SetEquippedWeaponHidden (el/los
	// BSFadeNode hijos de "WEAPON", no el propio hueso). Identidad + aviso
	// por log si no se encuentra (arma sin 3D cargado, o "WEAPON" sin
	// hijos). Pensado para capturar el punto de partida real del giro
	// justo antes de que el arma se vuelva réplica (ver
	// Throw::LaunchWeapon) -- da igual llamarla antes o después de
	// SetEquippedWeaponHidden, que no toca la transformación, solo la
	// visibilidad.
	RE::NiMatrix3 GetEquippedWeaponWorldRotation(RE::Actor& a_actor);

	// Rotación mundial actual del hueso "WEAPON" del esqueleto de
	// a_actor -- a diferencia de GetEquippedWeaponWorldRotation, existe
	// siempre (no depende de tener un arma equipada), así que es la única
	// referencia disponible durante el regreso, cuando el arma real ya
	// está desequipada del todo (ver CLAUDE.md,
	// WeaponManager::ThrowWeapon: el desequipado real se difiere unos
	// instantes tras el lanzamiento, pero llega mucho antes de que la
	// réplica esté cerca de volver). En teoría puede diferir de la
	// orientación real que tendría la malla equipada por el offset de
	// agarre que tenga el NIF entre el hueso y la malla -- medido en el
	// juego (log de diagnóstico ya retirado de Throw::LaunchWeapon, offset
	// real de (-0.00, -0.00, 0.00) grados XYZ) y confirmado despreciable
	// en este NIF en concreto, así que no hace falta ninguna corrección.
	// Identidad si el hueso no existe (no debería ocurrir con un
	// esqueleto normal).
	RE::NiMatrix3 GetHandBoneWorldRotation(RE::Actor& a_actor);

	// Punto 11: temblor de desprendimiento antes de iniciar el regreso
	// desde un objetivo clavado (duración a_duration, ver más abajo),
	// llamado desde 5.- RETURN/ReturnManager::BeginReturn antes de
	// arrancar el movimiento de vuelta en sí, nunca durante el vuelo.
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
	// a_elapsedSeconds debe ir de 0 (reposo, sin oscilación) a a_duration.
	// a_duration ya no es siempre Constants::kStickShudderDuration (cambio
	// de criterio 2026-08-07, ver CLAUDE.md): Return::BeginReturn puede
	// alargarlo por encima de ese mínimo en regresos cortos, para dar
	// tiempo a que la animación de Atrape se sincronice sin ralentizar el
	// vuelo en sí (ver Return::BeginReturnMovement) -- la fórmula del chirp
	// y de la envolvente de amplitud se estiran/comprimen sobre a_duration
	// igual que antes lo hacían sobre la constante fija. Mismo
	// comportamiento que TickSpin si el nodo de giro no existe todavía
	// (sin efecto, sin reintento explícito).
	void TickShudder(RE::TESObjectREFR& a_refr, const RE::NiMatrix3& a_baseRotation, float a_elapsedSeconds, float a_duration);

	// Fase 3 del plan OAR (_reference/PLAN-OAR.md), rediseñado 2026-08-05
	// (ver CLAUDE.md): activa/desactiva el TESGlobal
	// Constants::kThrowTriggerGlobalEditorID. a_actor ya no se usa para la
	// búsqueda (un Global no es per-actor), se conserva en la firma por
	// coherencia con el resto de funciones de este archivo y porque el
	// proyecto es de un solo jugador. Envuelta aquí (en vez de tocar el
	// Global directo desde WeaponManager) para que el nombre viva en un
	// solo sitio, mismo motivo que TickSpin/TickShudder.
	void SetThrowTrigger(RE::Actor& a_actor, bool a_active);

	// Mismo patrón que SetThrowTrigger, para Llamada: activa/desactiva
	// Constants::kCallTriggerGlobalEditorID.
	void SetCallTrigger(RE::Actor& a_actor, bool a_active);

	// Mismo patrón que SetThrowTrigger/SetCallTrigger, para Atrape: activa/
	// desactiva Constants::kCatchTriggerGlobalEditorID.
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

	// Zoom de cámara mientras dura State::kAiming -- arranca una rampa
	// manual propia (Constants::kAimZoomTransitionDuration) sobre
	// RE::PlayerCamera::RUNTIME_DATA2::worldFOV (estrecha el campo de
	// visión, mismo campo en primera y tercera persona) hacia el valor
	// objetivo (activar) o hacia el valor previo a activar (desactivar).
	// Ver Constants::kAimZoomFOVOffset para el porqué de FOV y no la
	// posición/zoom de la cámara en tercera persona (ThirdPersonState::
	// targetZoomOffset/currentZoomOffset, primer intento, descartado tras
	// confirmar en el juego que causaba que la cámara atravesara al
	// personaje varios metros, frenada solo por colisión real contra
	// geometría).
	//
	// Sin efecto (salvo actualizar el flag interno) si a_active coincide
	// con el estado ya activo -- evita sumar el offset dos veces si
	// BeginAiming se llama sin haber revertido antes (ver
	// WeaponManager::OnAimButtonDown, caso kAiming: reinicia el ciclo
	// llamando a BeginAiming de nuevo sin pasar por un SetAimZoom(false)
	// intermedio). Si ya hay una rampa en marcha (p. ej. se suelta el botón
	// antes de que termine la de entrada), la cancela y arranca la nueva
	// partiendo del valor real en ese instante, no del objetivo todavía sin
	// alcanzar -- sin salto visual.
	//
	// Función global sin parámetro RE::Actor& (a diferencia de
	// SetThrowTrigger/SetCallTrigger/SetCatchTrigger, que sí lo llevan por
	// coherencia con el resto del archivo pese a no usarlo, ver esas
	// funciones): RE::PlayerCamera es un singleton inherentemente ligado al
	// jugador, no un concepto por actor como una graph variable, así que no
	// aplica el mismo criterio aquí.
	void SetAimZoom(bool a_active);
}
