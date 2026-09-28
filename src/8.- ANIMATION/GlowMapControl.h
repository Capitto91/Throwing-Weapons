// Brillo del glow map del martillo (apagado / constante / pulso) según Settings.
// Escribe emissiveMult en el arma equipada y en la réplica cada tick.

#pragma once

namespace Animation::GlowMapControl
{
	// Arranca el bucle si no está en marcha; el arranque real va al hilo principal.
	// Lo llama EventManager al cargar/empezar partida, al equipar y tras la pantalla de carga.
	void EnsureRunning();
}
