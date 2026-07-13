// Instrumentación de diagnóstico TEMPORAL para investigar tirones
// reportados en el juego y averiguar si los causa este plugin o son
// comportamiento normal de Skyrim. No es parte de ninguna mecánica del
// arma — pensada para quitarse (o comentar sus usos) una vez recopilados
// los datos; ver CHANGELOG.md para el contexto de por qué se añadió.

#pragma once

#include <chrono>
#include <string_view>

namespace Perf
{
	// Cronómetro de ámbito (RAII): mide cuánto tarda en ejecutarse el
	// bloque en el que se declara y, si supera a_warnThreshold, lo escribe
	// en el log de SKSE con a_label para poder identificar qué punto del
	// código lo causó. Si no se supera el umbral, no escribe nada — para
	// no llenar el log de líneas irrelevantes en la inmensa mayoría de
	// los casos.
	class ScopedTimer
	{
	public:
		ScopedTimer(std::string_view a_label, std::chrono::microseconds a_warnThreshold) :
			label(a_label),
			warnThreshold(a_warnThreshold),
			start(std::chrono::steady_clock::now())
		{}

		ScopedTimer(const ScopedTimer&) = delete;
		ScopedTimer(ScopedTimer&&) = delete;
		ScopedTimer& operator=(const ScopedTimer&) = delete;
		ScopedTimer& operator=(ScopedTimer&&) = delete;

		~ScopedTimer()
		{
			const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start);
			if (elapsed >= warnThreshold) {
				logs::warn("[PERF] {} tardó {} us (umbral {} us)", label, elapsed.count(), warnThreshold.count());
			}
		}

	private:
		std::string_view                      label;
		std::chrono::microseconds             warnThreshold;
		std::chrono::steady_clock::time_point start;
	};
}
