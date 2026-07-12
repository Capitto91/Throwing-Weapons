// Implementación del controlador principal del plugin.
// Inicializa gestores de entrada, arma, eventos, física y otros sistemas.

#include "Plugin.h"

#include "2.- INPUT/InputManager.h"

namespace Plugin
{
    namespace
    {
        void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
        {
            // kInputLoaded: los dispositivos de entrada ya están listos para
            // registrar event sinks.
            if (a_message->type == SKSE::MessagingInterface::kInputLoaded) {
                Input::InputManager::GetSingleton()->Init();
            }
        }
    }

    void Init()
    {
        SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
    }
}