// Implementación del controlador principal del plugin.
// Inicializa gestores de entrada, arma, eventos, física y otros sistemas.

#include "Plugin.h"

#include "10.- EVENTS/EventManager.h"

namespace Plugin
{
    void Init()
    {
        Events::Init();
    }
}
