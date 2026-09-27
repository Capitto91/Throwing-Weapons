// Implementación del controlador principal del plugin.
// Inicializa gestores de entrada, arma, eventos, física y otros sistemas.

#include "Plugin.h"

#include "1.- CORE/Settings.h"
#include "10.- EVENTS/EventManager.h"

namespace Plugin
{
	void Init()
	{
		// Antes que nada: no depende del juego (solo lee el INI), y
		// Input::InputManager/Throw la consultan desde el primer uso.
		Settings::Load();
		Events::Init();
	}
}
