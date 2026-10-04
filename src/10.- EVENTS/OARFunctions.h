// Funciones propias registradas en Open Animation Replacer, en variante de tercera y de primera persona (sufijo 1P):
// OAR las llama en las anotaciones de Lanzar/Llamada/Atrape/golpe en salto y avisan en el hilo principal (AddTask).
#pragma once

namespace Events::OARFunctions
{
	// Registra las funciones en OAR. Lo llama EventManager en kPostLoad.
	// Sin OAR solo avisa por log.
	void RegisterAll();
}
