// Menú de configuración en el juego (SKSE Menu Framework, QTR-Modding).
// Añade una sección propia al menú del framework (F1 por defecto) para
// editar en vivo los ajustes de Settings y guardarlos en el INI. Opcional:
// sin el framework instalado, Register no hace nada y el plugin se configura
// solo por INI, como antes.

#pragma once

namespace UI::ConfigMenu
{
	// Registra la sección y sus páginas en el framework. Llamar una única
	// vez en kPostLoad: el header del framework guarda en caché el puntero
	// de cada función la primera vez que se usa, así que llamarlo antes de
	// que SKSE haya cargado SKSEMenuFramework.dll lo dejaría anulado para
	// toda la sesión.
	void Register();
}
