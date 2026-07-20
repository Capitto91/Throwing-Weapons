// Controla las animaciones especiales del arma.
// Gestiona giro durante vuelo, orientación antes del impacto y animación de agarre.

#pragma once

#include <functional>

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

	// Mejora Kratos #3 (PLAN-mejoras-kratos.md): transición visual breve al
	// llegar de verdad a la mano durante el regreso (no en un recall
	// instantáneo, ver WeaponManager::ReequipAfterCapture) -- clona
	// a_replicaRoot (NiObject::Clone(), nunca construyendo
	// NiCloningProcess a mano), lo engancha a a_handNode con un offset fijo
	// (Constants::kCaptureTransitionLocalOffset), y lo desengancha tras
	// Constants::kCaptureTransitionDuration (mismo patrón
	// hilo-que-duerme-y-reencola de todo el proyecto), momento en el que
	// llama a a_onComplete en el hilo principal. Puramente visual: no toca
	// el TESObjectREFR de la réplica ni su rigidbody de Havok. Si a_handNode
	// no es un NiNode o el clonado falla, no hace nada visible y llama a
	// a_onComplete de inmediato -- nunca bloquea el reequipado real.
	void PlayCaptureTransition(RE::NiAVObject& a_handNode, RE::NiAVObject& a_replicaRoot, std::function<void()> a_onComplete);
}
