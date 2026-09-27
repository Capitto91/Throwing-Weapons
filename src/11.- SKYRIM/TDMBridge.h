// Puente opcional hacia True Directional Movement (TDM, ersh1).
// Si TDM está instalado y el jugador tiene un objetivo fijado (target lock),
// Lanzar apunta a ese objetivo igual que TDM apunta sus propias flechas; si
// no está instalado, o no hay lock, el plugin se comporta exactamente igual
// que sin este módulo.

#pragma once

namespace TDMBridge
{
	// Pide la API de TDM (src/13.- EXTERNAL/TrueDirectionalMovement/, copia
	// literal de su header). Llamar en kPostLoad o después -- recomendación
	// del propio header: la DLL de TDM debe estar ya cargada en el proceso.
	// Sin TDM (o una versión sin API), solo avisa por log: todas las demás
	// funciones de este módulo devuelven "sin lock" a partir de ahí.
	void Init();

	// Actor que TDM tiene fijado ahora mismo, o nullptr si TDM no está
	// instalado, no hay lock activo o el actor ya no es válido. Solo desde el
	// hilo principal.
	[[nodiscard]] RE::NiPointer<RE::Actor> GetLockedTarget();

	// Punto del cuerpo al que apuntar en a_target, reproduciendo la elección
	// por defecto de TDM (DirectionalMovementHandler::GetTargetPoints con
	// uReticleAnchor = kBody, su valor por defecto): el hueso "targetName"
	// de la parte kTorso del BGSBodyPartData de su raza. Si no existe, el
	// mismo respaldo que usa TDM (TESObjectREFR::GetLookingAtLocation).
	// Diferencia conocida: TDM puede sobrescribir esos huesos por raza con
	// sus propios .toml (Data/SKSE/Plugins/TrueDirectionalMovement/) -- esa
	// configuración es interna de TDM, no está en su API, y no se replica.
	[[nodiscard]] RE::NiPoint3 GetTargetPoint(RE::Actor& a_target);

	// Copia literal de PredictAimProjectile de TDM (src/Utils.cpp, GPL-3.0,
	// a su vez adaptado de ringofblades.com/Blades/Code/PredictiveAim.cs),
	// la misma función que TDM usa para sus flechas con
	// uTargetLockArrowAimType = kPredict (su valor por defecto): dada la
	// velocidad actual del proyectil (a_projectileVelocity, solo se usa su
	// módulo), la reescribe para interceptar a un objetivo que se mueve a
	// a_targetVelocity, compensando además la caída por gravedad. a_gravity
	// es la magnitud POSITIVA de la aceleración hacia abajo (u/s²), igual
	// que en TDM. Tras compensar la gravedad el módulo puede diferir
	// ligeramente del original -- mismo comportamiento que TDM. Devuelve
	// false si no hay solución exacta (a_projectileVelocity queda igualmente
	// apuntando a la posición futura estimada, como en TDM).
	bool PredictAimProjectile(const RE::NiPoint3& a_projectilePos, const RE::NiPoint3& a_targetPosition, const RE::NiPoint3& a_targetVelocity, float a_gravity, RE::NiPoint3& a_projectileVelocity);
}
