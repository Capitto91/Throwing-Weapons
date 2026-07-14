// Declara el sistema de registro del plugin.
// Permite escribir mensajes de depuración para comprobar el funcionamiento
// interno del DLL durante el desarrollo.

#pragma once

namespace Logger
{
	// Inicializa el logger de SKSE (spdlog) para que el alias logs:: pueda
	// usarse en el resto del plugin. Debe llamarse una única vez al arrancar.
	void Init();
}
