// Arranque del plugin -- ver Plugin.h.

#include "Plugin.h"

#include "1.- CORE/Settings.h"
#include "10.- EVENTS/EventManager.h"

namespace Plugin
{
	void Init()
	{
		// Primero el INI: InputManager y Throw lo leen desde el primer uso.
		Settings::Load();
		Events::Init();
	}
}
