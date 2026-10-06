// Registro de rendimiento -- ver PerfMonitor.h.

#include "1.- CORE/PerfMonitor.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "6.- PHYSICS/PhysicsManager.h"

#include <chrono>
#include <optional>

namespace PerfMonitor
{
	namespace
	{
		using Clock = std::chrono::steady_clock;

		// Intervalo en curso: trabajo del plugin (µs), duración de los fotogramas (s) y máximos de los recuentos.
		struct Window
		{
			std::uint32_t frames{ 0 };
			double        workSum{ 0.0 };
			double        workMax{ 0.0 };
			double        frameSum{ 0.0 };
			double        frameMax{ 0.0 };
			std::size_t   loopsMax{ 0 };
			std::size_t   timersMax{ 0 };
		};

		// Solo los toca el hilo del hook. g_lastFrame vacío: no se está midiendo.
		Window                           g_window;
		std::optional<Clock::time_point> g_lastFrame;

		// Escribe la línea del intervalo con los recuentos de ahora y empieza otro.
		void Flush(std::size_t a_loops, std::size_t a_timers)
		{
			const double workAvg = g_window.workSum / g_window.frames;
			const double frameAvg = g_window.frameSum / g_window.frames;
			const double workPercent = frameAvg > 0.0 ? workAvg / (frameAvg * 1.0e6) * 100.0 : 0.0;

			logs::info("Rendimiento ({:.0f} s, {} fotogramas): plugin media {:.0f} µs, máx {:.0f} µs | fotograma medio {:.1f} ms ({:.2f} %), máx {:.1f} ms | bucles {} (máx {}) | temporizadores {} (máx {})",
				g_window.frameSum, g_window.frames, workAvg, g_window.workMax, frameAvg * 1000.0, workPercent, g_window.frameMax * 1000.0,
				a_loops, g_window.loopsMax, a_timers, g_window.timersMax);

			g_window = {};
		}
	}

	void RecordFrame(double a_workMicroseconds)
	{
		const auto now = Clock::now();
		const auto loops = Physics::GetActiveLoopCount();
		const auto timers = Scheduler::GetPendingTimerCount();

		// Duración desde el fotograma medido anterior; la primera llamada o un hueco largo (pausa, carga) no cuenta.
		const double frameSeconds = g_lastFrame ? std::chrono::duration<double>(now - *g_lastFrame).count() : 0.0;
		g_lastFrame = now;
		if (frameSeconds <= 0.0 || frameSeconds > Constants::kPerformanceLogMaxFrameSeconds) {
			return;
		}

		++g_window.frames;
		g_window.workSum += a_workMicroseconds;
		g_window.workMax = (std::max)(g_window.workMax, a_workMicroseconds);
		g_window.frameSum += frameSeconds;
		g_window.frameMax = (std::max)(g_window.frameMax, frameSeconds);
		g_window.loopsMax = (std::max)(g_window.loopsMax, loops);
		g_window.timersMax = (std::max)(g_window.timersMax, timers);

		if (g_window.frameSum >= Constants::kPerformanceLogIntervalSeconds) {
			Flush(loops, timers);
		}
	}

	void Stop()
	{
		if (!g_lastFrame) {
			return;
		}

		g_lastFrame.reset();
		g_window = {};
	}
}
