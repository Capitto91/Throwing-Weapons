// Hook de PlayerCharacter::Update (vtable, encadenado con los de otros mods): en cada fotograma sin
// pausa avanza el reloj de juego, los temporizadores de Scheduler y los bucles de Physics. Lo instala Plugin::Init.

#pragma once

namespace FrameHook
{
	// Instala el hook; false en VR (posición de Update en su vtable sin verificar), donde Physics sigue con hilos.
	bool Install();

	// true si el hook está instalado. Lo consultan Physics::StartTickLoop y Scheduler::After para elegir fotogramas o hilos.
	[[nodiscard]] bool IsInstalled() noexcept;

	// Segundos de juego sin pausa (con el multiplicador de tiempo); sin hook, segundos reales.
	// Reloj de CatchSync y WeaponManager para la sincronía con Catch.hkx. Seguro desde cualquier hilo.
	[[nodiscard]] double Now() noexcept;
}
