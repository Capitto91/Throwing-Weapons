// Comprobación de requisitos en el log: versión del juego, SKSE, Address Library, mods SKSE
// necesarios y opcionales con su versión, y el .esp del mod.

#pragma once

namespace Requirements
{
	// Guarda a_skse y registra juego, SKSE y Address Library. Lo llama SKSEPluginLoad.
	void Init(const SKSE::LoadInterface* a_skse);

	// Registra los plugins SKSE requeridos y opcionales (todos ya cargados). Lo llama EventManager en kPostLoad.
	void CheckPlugins();

	// Comprueba que el .esp está cargado y sus formularios se resuelven. Lo llama EventManager en kDataLoaded.
	void CheckPluginFile();
}
