// Réplica del arma movida a mano cada tick (sin RE::Projectile): creación, bucle de tick,
// sincronización con Havok y borrado. La usan Throw, Return, Combat y los VFX.

#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace Physics
{
	// Callback de cada tick (hilo principal) con la réplica y los segundos desde el tick anterior
	// (de juego con FrameHook, reales sin él; tope Constants::kMaxTickDeltaSeconds). false para terminar.
	using TickCallback = std::function<bool(RE::TESObjectREFR&, float)>;

	// Token para detener un bucle de tick desde fuera.
	using TickToken = std::shared_ptr<std::atomic<bool>>;

	// Aviso con la réplica lista para moverse, o handle inválido si su 3D no cargó.
	using ReadyCallback = std::function<void(RE::ObjectRefHandle)>;

	// Crea la réplica del arma en a_position, espera a su 3D, la pone en kKeyframed
	// y llama a a_onReady.
	void SpawnReplica(RE::Actor* a_actor, RE::TESObjectWEAP* a_weapon, const RE::NiPoint3& a_position, ReadyCallback a_onReady);

	// Sincroniza Havok y el nodo visual tras SetPosition/SetAngle.
	void SyncHavok(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_position, const RE::NiPoint3& a_angle);

	// Lleva a_refr a a_position (posición lógica, Havok y nodo visual) con su ángulo actual.
	// La usan los bucles de tick de la ida, el regreso y el clavado.
	void MoveTo(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_position);

	// Llama a a_callback en el hilo principal en cada fotograma sin pausa (FrameHook) o, sin hook,
	// cada Constants::kTickInterval. Empieza en el tick siguiente. Devuelve el token para cancelarlo.
	[[nodiscard]] TickToken StartTickLoop(RE::ObjectRefHandle a_handle, TickCallback a_callback);

	// Detiene un bucle desde fuera y vacía a_token; sin efecto si ya estaba vacío.
	void CancelTickLoop(TickToken& a_token);

	// Avanza un fotograma los bucles de StartTickLoop con a_deltaSeconds de juego.
	// Lo llama FrameHook en cada PlayerCharacter::Update sin pausa.
	void RunFrame(float a_deltaSeconds);

	// Borra una referencia creada por el plugin (réplica, destello o hazard) con Disable + SetDelete;
	// si tenía un bucle de tick, para solo.
	void DestroyReference(RE::ObjectRefHandle a_handle);
}
