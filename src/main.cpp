// Punto de entrada principal del plugin SKSE.
// Inicializa la DLL, registra el plugin en Skyrim y ejecuta la inicialización
// de los diferentes sistemas internos del mod.

#include "1.- CORE/Logger.h"
#include "Plugin.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    Logger::Init();
    Plugin::Init();

    return true;
}