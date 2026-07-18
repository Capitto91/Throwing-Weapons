// Controla las animaciones especiales del arma.
// Gestiona giro durante vuelo, orientación antes del impacto y animación de agarre.

#pragma once

// Punto 10 de Mecanica del arma.txt: giro sobre sí misma durante el vuelo.
// El giro vive horneado en el propio NIF del arma como un
// NiTransformController "suelto" (sin NiControllerManager/NiControllerSequence
// de por medio, a diferencia de un primer diseño descartado antes de
// implementarse) colgado de un nodo hijo dedicado
// (Constants::kWeaponSpinNodeName) — no del nodo raíz, que el propio
// código reescribe cada tick (SetAngle/Update3DPosition) para mover la
// réplica y competiría con el controlador si compartieran nodo. Inactivo
// por defecto (sin el flag kActive), así que ni la versión equipada ni la
// tirada en el suelo giran solas; arrancarlo/pararlo por código es
// puramente visual, no toca el ángulo lógico del TESObjectREFR ni la
// rotación de Havok.

namespace Animation
{
	// Arranca el NiTransformController del nodo Constants::kWeaponSpinNodeName
	// de a_refr. Sin efecto (con un aviso en el log) si su NIF todavía no
	// tiene ese nodo o su controlador.
	void StartSpin(RE::TESObjectREFR& a_refr);

	// Como StartSpin, pero pensada para el instante justo después de crear
	// la réplica (Physics::SpawnReplica): Get3D() ya no es nulo en ese
	// punto, pero eso no garantiza que todo el subárbol -- incluido el
	// nodo de giro -- haya terminado de cargar. Reintenta unas pocas veces
	// (Constants::kWeaponSpinNodeWaitAttempts) antes de rendirse, con el
	// mismo patrón hilo-que-duerme-y-reencola que Physics::SpawnReplica.
	void StartSpinWhenReady(RE::ObjectRefHandle a_handle);

	// Detiene el giro si estaba en marcha.
	void StopSpin(RE::TESObjectREFR& a_refr);

	// Transformación mundial a usar como base para efectos que deben
	// acompañar el giro visual (p. ej. la estela, ver
	// 8.- ANIMATION/WeaponTrail): el nodo raíz de la réplica (a_root) no
	// gira durante el vuelo -- SetAngle nunca cambia, solo se mueve la
	// posición -- así que su world transform por sí sola no refleja el
	// giro, que vive solo en el NiTransformController del nodo hijo
	// Constants::kWeaponSpinNodeName. Devuelve la transformación de ese
	// nodo si existe (misma posición que el raíz, rotación actualizada
	// por el propio controlador cada fotograma), o la del raíz tal cual
	// si el nodo de giro no existe todavía (NIF sin cargar del todo, o
	// sin la convención de giro).
	RE::NiTransform GetVisualTransform(RE::NiAVObject& a_root);
}
