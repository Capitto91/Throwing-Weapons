// Define la clase principal del plugin.
// Actúa como punto central de inicialización y coordinación de todos los sistemas
// necesarios para el funcionamiento del arma lanzable.

#pragma once

namespace Plugin
{
	// Registra los listeners de SKSE necesarios para arrancar el resto de
	// sistemas del plugin. Debe llamarse una única vez desde SKSEPluginLoad.
	void Init();
}
