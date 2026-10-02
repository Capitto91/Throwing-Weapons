// Regreso de la réplica a la mano: temblor si estaba clavada, curva con aceleración creciente
// y avisos para el gesto de Atrape y la llegada. Lo arranca WeaponManager::BeginReturn.

#pragma once

#include "6.- PHYSICS/PhysicsManager.h"

#include <functional>
#include <memory>
#include <optional>

namespace Return
{
	// Sincronía del regreso con Catch.hkx: fija la hora de llegada del arma cuando el gesto empieza
	// (reloj FrameHook::Now). La crea WeaponManager::BeginReturn; la usan los bucles y BeginCatchAnimation.
	class CatchSync
	{
	public:
		// a_leadSeconds: segundos desde el inicio de Catch.hkx hasta su anotación de mano cerrada.
		explicit CatchSync(float a_leadSeconds) noexcept :
			leadSeconds(a_leadSeconds)
		{}

		[[nodiscard]] float GetLeadSeconds() const noexcept { return leadSeconds; }

		// Catch.hkx acaba de empezar: la llegada queda fijada a ahora + GetLeadSeconds().
		// Lo llama WeaponManager::BeginCatchAnimation si el grafo acepta el attackStart.
		void OnCatchStarted();

		// Catch.hkx no se reproducirá: el arma vuelve a su ritmo natural, sin esperarlo.
		// Lo llama WeaponManager::BeginCatchAnimation si el grafo rechaza el attackStart.
		void Release() noexcept { released = true; }

		// Marca que ya se pidió el Atrape (onApproaching). Lo llaman los bucles del regreso.
		void               MarkRequested();
		[[nodiscard]] bool IsRequested() const noexcept { return requestedAt.has_value(); }

		// Segundos hasta la llegada fijada; vacío mientras Catch.hkx no haya empezado.
		[[nodiscard]] std::optional<float> GetSecondsToDeadline() const;

		// true si el arma ya no espera a Catch.hkx: liberada, o pedida y sin empezar pasado
		// Constants::kCatchStartTimeout. Lo consulta el temblor para decidir el despegue.
		[[nodiscard]] bool IsFree();

	private:
		float                 leadSeconds;
		std::optional<double> requestedAt;
		std::optional<double> deadline;
		bool                  released{ false };
	};

	struct ReturnCallbacks
	{
		// Token del bucle del regreso, para cancelarlo desde fuera.
		std::function<void(Physics::TickToken)> onTickStarted;

		// Aviso cuando la llegada prevista queda a CatchSync::GetLeadSeconds() o menos.
		// WeaponManager arranca aquí el gesto de Atrape.
		std::function<void()> onApproaching;

		// El arma ha llegado a la mano (Constants::kReturnArrivalDistance); la réplica se detiene.
		// El reequipado lo hace la anotación de Catch.hkx, no este aviso.
		std::function<void()> onArrived;
	};

	// Inicia el regreso de a_replicaHandle a la mano de a_player (curva que sigue a la mano y giro).
	// Con a_wasStuck, primero tiembla hasta que el vuelo natural llegue a la anotación de Catch.hkx.
	void BeginReturn(RE::Actor* a_player, RE::ObjectRefHandle a_replicaHandle, bool a_wasStuck, std::shared_ptr<CatchSync> a_catchSync, ReturnCallbacks a_callbacks);
}
