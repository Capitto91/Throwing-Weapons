// Destello de un solo uso en las dos manos del jugador (BGSArtObject vía ApplyArtObject).

#pragma once

namespace RE
{
	class Actor;
}

namespace Animation
{
	// Aplica el art object en el hueso de cada mano de a_actor; el motor lo retira solo.
	// Lo llama WeaponManager al lanzar y al atrapar. Hilo principal.
	void TriggerHandGlow(RE::Actor& a_actor);
}
