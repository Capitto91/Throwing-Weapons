// Gestiona la simulación física del arma.
// Controla velocidad, gravedad, movimiento y actualización de posición.

#pragma once

#include <atomic>
#include <functional>
#include <memory>

// Primitiva de movimiento manual compartida por la ida (4.- THROW) y la
// vuelta (5.- RETURN): crea una réplica visual del arma (TESObjectREFR
// normal, nunca un RE::Projectile) y permite moverla a mano, tick a tick,
// sin que compita con ninguna lógica de vuelo balístico nativa — ver
// CLAUDE.md, "Arquitectura de física de proyectiles" y "patrón de control
// manual".

namespace Physics
{
	// Invocado cada tick en el hilo principal con la réplica y el tiempo
	// transcurrido desde el tick anterior (Constants::kTickDeltaSeconds,
	// fijo). El propio callback es responsable de llamar
	// SetPosition/SetAngle y luego SyncHavok; devuelve false para terminar
	// el bucle (el propio callback ya se habrá encargado de cualquier
	// limpieza o notificación en ese caso). El bucle también se detiene
	// solo si la réplica deja de existir (p. ej. otro código la borró con
	// DestroyReplica mientras tanto).
	using TickCallback = std::function<bool(RE::TESObjectREFR&, float)>;

	// Token de cancelacion de un bucle de tick en marcha (ver
	// StartTickLoop). El propio bucle ya se detiene solo cuando el
	// callback devuelve false o la replica deja de existir, pero eso solo
	// cubre el caso de que sea el propio tick quien decide parar. El
	// regreso (5.- RETURN) necesita lo contrario: al pulsar el boton de
	// recuperar mientras el arma todavia vuela o sigue clavada en un
	// actor, hay que detener ese bucle desde fuera (otro punto de la
	// ejecucion en el hilo principal, no el propio tick) antes de arrancar
	// el del regreso -- sin este token, los dos bucles escribirian la
	// posicion de la misma replica cada tick.
	using TickToken = std::shared_ptr<std::atomic<bool>>;

	// Invocado en el hilo principal cuando la réplica recién creada está
	// lista para moverse (3D cargado y en modo Havok "keyframed"), o con un
	// handle inválido si el 3D nunca llegó a cargar a tiempo.
	using ReadyCallback = std::function<void(RE::ObjectRefHandle)>;

	// Crea la réplica visual del arma en a_position (PlaceObjectAtMe sobre
	// a_actor + reposicionado exacto) y espera a que su 3D termine de
	// cargar en segundo plano (sondeo con el mismo patrón
	// hilo-que-duerme-y-reencola que StartTickLoop) antes de ponerla en
	// modo Havok "keyframed" (sin fuerzas/gravedad, con colisión) y llamar
	// a a_onReady.
	void SpawnReplica(RE::Actor* a_actor, RE::TESObjectWEAP* a_weapon, const RE::NiPoint3& a_position, ReadyCallback a_onReady);

	// Sincroniza el bhkRigidBody de Havok con la posición/rotación ya
	// puestas con SetPosition/SetAngle (TESObjectREFR no lo hace por sí
	// solo) y refresca el nodo visual (Update3DPosition). Llamar siempre
	// después de SetPosition/SetAngle, nunca antes.
	void SyncHavok(RE::TESObjectREFR& a_refr, const RE::NiPoint3& a_position, const RE::NiPoint3& a_angle);

	// Arranca el bucle de tick manual sobre a_handle: un único hilo que
	// duerme Constants::kTickInterval y reencola en el hilo principal en
	// bucle (nunca AddTask reentrante desde dentro de sí mismo — congela el
	// juego, ver CLAUDE.md), llamando a a_callback cada paso. Devuelve el
	// token de cancelación de este bucle en concreto (ver TickToken); el
	// llamante es responsable de guardarlo si necesita poder detenerlo
	// desde fuera más adelante.
	[[nodiscard]] TickToken StartTickLoop(RE::ObjectRefHandle a_handle, TickCallback a_callback);

	// Detiene un bucle en marcha desde fuera de sí mismo (ver TickToken).
	// Sin efecto sobre un token vacío o ya cancelado.
	void CancelTickLoop(const TickToken& a_token);

	// Borra la réplica (Disable + SetDelete) — paso final común a ida y
	// vuelta, y forma de cancelar un StartTickLoop en marcha desde fuera:
	// el siguiente tick encontrará el handle inválido y parará solo.
	void DestroyReplica(RE::ObjectRefHandle a_handle);
}
