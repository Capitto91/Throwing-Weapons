// Ejecuta una función una sola vez en el hilo principal pasado un tiempo de juego, con cancelación.

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace Scheduler
{
	// Token para cancelar una tarea de After.
	using CancelToken = std::shared_ptr<std::atomic<bool>>;

	// Ejecuta a_callback (AddTask) pasado a_delay de juego: con FrameHook se congela con la pausa;
	// sin él, tiempo real. Seguro desde cualquier hilo y desde dentro de una tarea.
	[[nodiscard]] CancelToken After(std::chrono::milliseconds a_delay, std::function<void()> a_callback);

	// Cancela una tarea de After y vacía a_token; sin efecto si ya se disparó o el token está vacío.
	void Cancel(CancelToken& a_token);

	// Descuenta a_deltaSeconds de juego a las tareas de After y encola las vencidas.
	// Lo llama FrameHook en cada PlayerCharacter::Update sin pausa.
	void RunFrame(float a_deltaSeconds);

	// Tareas de After en cuenta atrás (incluidas las recién creadas). Solo desde el hilo del hook:
	// lo llama PerfMonitor tras RunFrame.
	[[nodiscard]] std::size_t GetPendingTimerCount();
}
