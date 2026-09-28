// Ejecuta una función una sola vez en el hilo principal pasado un tiempo, con cancelación.

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace Scheduler
{
	// Token para cancelar una tarea de After.
	using CancelToken = std::shared_ptr<std::atomic<bool>>;

	// Ejecuta a_callback en el hilo principal pasado a_delay, salvo que se cancele antes.
	// Seguro desde cualquier hilo y desde dentro de una tarea.
	[[nodiscard]] CancelToken After(std::chrono::milliseconds a_delay, std::function<void()> a_callback);

	// Cancela una tarea de After; sin efecto si ya se disparó o el token está vacío.
	void Cancel(const CancelToken& a_token);
}
