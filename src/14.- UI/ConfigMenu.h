// Sección "Throwable Kyne's Thunder" en SKSE Menu Framework: edita Settings y guarda el INI.
// Sin el framework instalado no hace nada.

#pragma once

namespace UI::ConfigMenu
{
	// Registra la sección y sus páginas. Lo llama EventManager en kPostLoad, nunca antes.
	void Register();
}
