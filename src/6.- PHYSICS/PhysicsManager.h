// Réplica del arma movida a mano cada tick (sin RE::Projectile): creación, bucle de tick,
// sincronización con Havok y borrado. La usan Throw, Return, Combat y los VFX.

#pragma once

#include <atomic>
#include <functional>
#include <memory>

namespace Physics
{
	// Callback de cada tick (hilo principal) con la réplica y el tiempo transcurrido.
	// Devuelve false para terminar; el bucle también para si la réplica deja de existir.
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

	// Llama a a_callback cada Constants::kTickInterval en el hilo principal.
	// Devuelve el token para cancelarlo.
	[[nodiscard]] TickToken StartTickLoop(RE::ObjectRefHandle a_handle, TickCallback a_callback);

	// Detiene un bucle desde fuera; sin efecto si el token está vacío.
	void CancelTickLoop(const TickToken& a_token);

	// Borra la réplica (Disable + SetDelete); su bucle de tick para solo.
	void DestroyReplica(RE::ObjectRefHandle a_handle);
}
