// Temporizador diferido centralizado.
// Sustituye al patrón repetido a mano "std::thread + sleep_for +
// SKSE::GetTaskInterface()->AddTask" que aparecía suelto en varios sitios
// del proyecto (WeaponManager, sobre todo) -- misma técnica exacta (ver
// CLAUDE.md, "Arquitectura de física de proyectiles", el mismo patrón
// hilo-que-duerme-y-reencola ya usado por Physics::StartTickLoop), pero en
// un solo sitio reutilizable, con cancelación opcional.

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>

namespace Scheduler
{
	// Token de cancelación de una tarea diferida en marcha -- mismo patrón
	// que Physics::TickToken (un std::atomic<bool> compartido). No hace
	// falta guardarlo si la tarea nunca va a necesitar cancelarse.
	using CancelToken = std::shared_ptr<std::atomic<bool>>;

	// Programa a_callback para ejecutarse en el hilo principal (vía
	// SKSE::GetTaskInterface()->AddTask) pasados a_delay reales -- a
	// diferencia de Physics::StartTickLoop (que repite indefinidamente
	// hasta que el callback devuelve false o se cancela), esto dispara una
	// única vez. Si el token devuelto se cancela (Cancel) antes de que se
	// cumpla el plazo, a_callback nunca llega a llamarse -- la
	// comprobación ocurre dentro de la propia tarea de AddTask, ya en el
	// hilo principal.
	[[nodiscard]] CancelToken After(std::chrono::milliseconds a_delay, std::function<void()> a_callback);

	// Cancela una tarea programada con After -- sin efecto sobre un token
	// vacío, ya cancelado, o cuya tarea ya se disparó (mismo contrato que
	// Physics::CancelTickLoop).
	void Cancel(const CancelToken& a_token);
}
