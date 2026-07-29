// Gestiona el registro y distribución de eventos del plugin.
// Conecta Skyrim, SKSE y los sistemas internos del arma.

#pragma once

namespace Events
{
	// Registra los listeners de SKSE y del motor (carga de partida, equipar
	// objetos) necesarios para mantener sincronizados los sistemas internos
	// del plugin con el estado real del juego. Debe llamarse una única vez
	// desde Plugin::Init().
	void Init();

	// Registra Events::ThrowReleaseWatcher sobre el jugador si todavía no se
	// ha hecho (idempotente, seguro llamar varias veces). Se llama de forma
	// perezosa desde WeaponManager::BeginAiming en vez de depender de
	// kNewGame/kPostLoadGame -- ninguno de los dos se dispara al entrar a la
	// partida vía `coc` desde la consola del menú principal (confirmado en
	// el juego, ver _reference/PLAN-OAR.md Fase 3), un método de prueba
	// habitual que se salta el flujo normal de nueva partida/cargar partida.
	void EnsureAnimationSinksRegistered();
}
