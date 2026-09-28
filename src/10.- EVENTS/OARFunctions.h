// Funciones propias registradas en Open Animation Replacer: OAR las llama en las anotaciones
// de liberación de Lanzar/Llamada/Atrape y avisan a WeaponManager.
#pragma once

namespace Events::OARFunctions
{
	// Registra las funciones en OAR. Lo llama EventManager en kPostLoad.
	// Sin OAR solo avisa por log.
	void RegisterAll();
}
