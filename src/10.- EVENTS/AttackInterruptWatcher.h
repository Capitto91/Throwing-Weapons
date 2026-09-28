// Espera a que el grafo termine de volver a reposo tras cortar un ataque (N-ésimo attackStop)
// antes de lanzar/llamar. Lo usa WeaponManager::InterruptAttackThen.
#pragma once

#include <functional>

namespace Events::AttackInterruptWatcher
{
	// Arma el vigilante; a_onReady se ejecuta una vez en el hilo principal al llegar la señal.
	// No se ejecuta si no llega dentro de Constants::kAttackInterruptWatchWindow.
	void Arm(RE::Actor& a_actor, std::function<void()> a_onReady);

	// Desarma el vigilante sin ejecutar el callback pendiente.
	void Disarm();
}
