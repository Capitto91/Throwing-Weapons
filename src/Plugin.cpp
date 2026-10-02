// Arranque del plugin -- ver Plugin.h.

#include "Plugin.h"

#include "1.- CORE/FrameHook.h"
#include "1.- CORE/Settings.h"
#include "10.- EVENTS/EventManager.h"

namespace Plugin
{
	void Init()
	{
		// Primero el INI: InputManager y Throw lo leen desde el primer uso.
		Settings::Load();
		// Antes de cualquier bucle de Physics, que elige entre fotogramas e hilos al crearse.
		FrameHook::Install();
		Events::Init();
	}
}
