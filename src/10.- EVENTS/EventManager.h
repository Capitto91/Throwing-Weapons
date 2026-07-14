// Gestiona el registro y distribución de eventos del plugin.
// Conecta Skyrim, SKSE y los sistemas internos del arma.

#pragma once

namespace Events
{
	// Registra los listeners de SKSE y del motor (carga de partida, equipar
	// objetos) necesarios para mantener sincronizados los sistemas internos
	// del plugin con el estado real del juego. Debe llamarse una única vez
	// desde Plugin::Init().
	void Init();
}
