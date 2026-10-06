// Registro de rendimiento del plugin ([Debug] PerformanceLog): recibe de FrameHook lo que tarda nuestro trabajo en cada
// fotograma y escribe en el log, cada Constants::kPerformanceLogIntervalSeconds, medias, máximos y recuentos.

#pragma once

namespace PerfMonitor
{
	// Un fotograma medido: a_workMicroseconds de Scheduler::RunFrame + Physics::RunFrame. La duración del fotograma se
	// mide aquí, entre llamadas. Lo llama FrameHook con la opción activa.
	void RecordFrame(double a_workMicroseconds);

	// Descarta el intervalo a medias. Lo llama FrameHook con la opción apagada.
	void Stop();
}
