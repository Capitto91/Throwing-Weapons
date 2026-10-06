// Arranque del plugin -- ver Plugin.h.

#include "Plugin.h"

#include "1.- CORE/CastHook.h"
#include "1.- CORE/FrameHook.h"
#include "1.- CORE/Settings.h"
#include "10.- EVENTS/EventManager.h"
#include "8.- ANIMATION/CameraKick.h"

namespace Plugin
{
	void Init()
	{
		// Primero el INI: InputManager y Throw lo leen desde el primer uso.
		Settings::Load();
		// Antes de cualquier bucle de Physics, que elige entre fotogramas e hilos al crearse.
		FrameHook::Install();
		// Deniega Lightning Dash antes de lanzarlo si no puede desplazar al jugador.
		CastHook::Install();
		// Golpe de cámara del atrape, sumado a la rotación de la cámara del jugador.
		Animation::CameraKick::Install();
		Events::Init();
	}
}
