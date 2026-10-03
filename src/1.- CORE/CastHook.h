// Hook de ActorMagicCaster::CheckCast (vtable, encadenado con los de otros mods): impide lanzar
// Lightning Dash cuando no puede desplazar al jugador (WeaponManager::CanCastLightningDash). Lo instala Plugin::Init.

#pragma once

namespace CastHook
{
	// Instala el hook (misma posición en SE, AE y VR según ActorMagicCaster.h).
	void Install();
}
