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
}
