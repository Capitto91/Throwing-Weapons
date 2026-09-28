// Punto de entrada SKSE: inicia SKSE, el log y Plugin::Init.

#include "1.- CORE/Logger.h"
#include "1.- CORE/Requirements.h"
#include "Plugin.h"

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    Logger::Init();
    Requirements::Init(skse);
    Plugin::Init();

    return true;
}
