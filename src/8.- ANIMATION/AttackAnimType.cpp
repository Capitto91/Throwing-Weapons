// Ataques de maza con el arma arrojadiza -- ver AttackAnimType.h.

#include "8.- ANIMATION/AttackAnimType.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Scheduler.h"
#include "3.- WEAPON/WeaponManager.h"

#include <mutex>
#include <unordered_map>

namespace Animation::AttackAnimType
{
	namespace
	{
		// Tipo original del registro de cada arma cuyo tipo se ha tocado.
		// Compartido entre el hilo principal (EnsureRegistered/Restore) y
		// los hilos de animación (ProcessEvent) -- protegido por mutex.
		std::mutex                                              mutex;
		std::unordered_map<RE::TESObjectWEAP*, RE::WEAPON_TYPE> originalTypes;

		// Paso a maza pendiente (ver RequestPromote). Protegido por mutex.
		Scheduler::CancelToken promoteToken;
		int                    promoteAttempts{ 0 };

		RE::TESObjectWEAP* GetEquippedThrowableWeapon(RE::Actor& a_actor)
		{
			// false = mano derecha / mano principal.
			auto* rightHand = a_actor.GetEquippedObject(false);
			auto* weapon = rightHand ? rightHand->As<RE::TESObjectWEAP>() : nullptr;
			return weapon && weapon->HasKeywordString(Constants::kThrowableWeaponKeyword) ? weapon : nullptr;
		}

		// Requiere el mutex tomado.
		void SetType(RE::TESObjectWEAP* a_weapon, RE::WEAPON_TYPE a_type, std::string_view a_reason)
		{
			if (a_weapon->GetWeaponType() == a_type) {
				return;
			}

			logs::info("AttackAnimType: '{}' tipo {} -> {} ({}).",
				a_weapon->GetName(), std::to_underlying(a_weapon->GetWeaponType()), std::to_underlying(a_type), a_reason);
			a_weapon->weaponData.animationType = a_type;
		}

		// Requiere el mutex tomado.
		void RestoreAllExcept(RE::TESObjectWEAP* a_keep, std::string_view a_reason)
		{
			for (auto& [weapon, original] : originalTypes) {
				if (weapon != a_keep) {
					SetType(weapon, original, a_reason);
				}
			}
		}

		// Tipo de maza solo con el arma desenvainada del todo y el ciclo en
		// reposo (kInHand). En cualquier fase del ciclo (lanzando, lanzada,
		// llamando, regresando) se queda en el tipo original: la réplica se
		// crea (PlaceObjectAtMe) y el arma se desequipa/reequipa con el
		// tipo del registro, igual que antes de este módulo.
		bool WantsDrawnType(RE::Actor& a_actor)
		{
			return a_actor.AsActorState()->GetWeaponState() == RE::WEAPON_STATE::kDrawn &&
			       Weapon::WeaponManager::GetSingleton()->GetState() == Weapon::State::kInHand;
		}

		// El modelo equipado ya cuelga del hueso "WEAPON" de la mano (3D de
		// tercera persona, que existe también en primera).
		bool IsModelInHand(RE::Actor& a_actor)
		{
			auto* root = a_actor.Get3D(false);
			auto* weaponBone = root ? root->GetObjectByName("WEAPON") : nullptr;
			auto* asNode = weaponBone ? weaponBone->AsNode() : nullptr;
			return asNode && !asNode->GetChildren().empty();
		}

		void Promote();

		// Requiere el mutex tomado. Programa Promote en el hilo principal
		// (Scheduler: hilo propio que reencola con AddTask, seguro desde
		// cualquier hilo, también desde dentro de una tarea).
		void RequestPromote()
		{
			promoteToken = Scheduler::After(Constants::kDrawnTypePromoteRetryInterval, Promote);
		}

		// Paso a maza, en el hilo principal y solo con el modelo ya en la
		// mano. Comprobado en el juego (2026-09-28): cambiarlo en el propio
		// evento "weaponDraw" (el instante en que el motor pasa el modelo
		// de la cadera a la mano) dejaba el modelo colgado en la cadera y
		// la mano vacía -- el motor lo buscaba en el nodo de cadera del
		// tipo nuevo (maza), no lo encontraba y no lo movía; y la réplica
		// del lanzamiento no se veía.
		void Promote()
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			auto* weapon = player ? GetEquippedThrowableWeapon(*player) : nullptr;

			std::lock_guard lock(mutex);
			promoteToken.reset();
			if (!weapon || !WantsDrawnType(*player)) {
				return;
			}

			if (!IsModelInHand(*player)) {
				if (++promoteAttempts < Constants::kDrawnTypePromoteMaxAttempts) {
					RequestPromote();
				} else {
					logs::warn("AttackAnimType: el modelo del arma no ha llegado a la mano tras {} intentos, se queda con el tipo original.", promoteAttempts);
				}
				return;
			}

			SetType(weapon, Constants::kDrawnAnimationWeaponType, "Promote");
		}

		void Sync(RE::Actor& a_actor, std::string_view a_reason)
		{
			auto* weapon = GetEquippedThrowableWeapon(a_actor);

			std::lock_guard lock(mutex);

			// Sin el arma en la mano (desequipada por el jugador, o fuera
			// de ella por el propio ciclo de lanzamiento): ninguna debe
			// quedarse como maza.
			RestoreAllExcept(weapon, a_reason);
			if (!weapon) {
				return;
			}

			const auto original = originalTypes.try_emplace(weapon, weapon->GetWeaponType()).first->second;
			if (!WantsDrawnType(a_actor)) {
				Scheduler::Cancel(promoteToken);
				promoteToken.reset();
				SetType(weapon, original, a_reason);
				return;
			}

			// El paso a maza nunca se hace aquí directamente: ver Promote.
			if (weapon->GetWeaponType() != Constants::kDrawnAnimationWeaponType && !promoteToken) {
				promoteAttempts = 0;
				RequestPromote();
			}
		}

		class Sink final : public RE::BSTEventSink<RE::BSAnimationGraphEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::BSAnimationGraphEvent* a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>*) override
			{
				// Solo se registra en los grafos del jugador (EnsureRegistered),
				// pero se comprueba igualmente el emisor.
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (a_event && player && a_event->holder == player) {
					Sync(*player, a_event->tag.c_str());
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		Sink sink;
	}

	void EnsureRegistered(RE::Actor& a_actor)
	{
		// Todos los grafos, no solo el primero como
		// Actor::AddAnimationGraphEventSink: el jugador tiene uno de
		// tercera y otro de primera persona, y en primera persona los
		// eventos salen del segundo. BSTEventSource::AddEventSink ya
		// ignora un sink repetido.
		RE::BSAnimationGraphManagerPtr graphManager;
		a_actor.GetAnimationGraphManager(graphManager);
		if (!graphManager) {
			logs::warn("AttackAnimType::EnsureRegistered: '{}' sin grafo de animación todavía.", a_actor.GetName());
			return;
		}

		for (const auto& graph : graphManager->graphs) {
			if (graph) {
				graph->GetEventSource<RE::BSAnimationGraphEvent>()->AddEventSink(&sink);
			}
		}

		Sync(a_actor, "EnsureRegistered");
	}

	void Restore(RE::TESObjectWEAP* a_weapon)
	{
		if (!a_weapon) {
			return;
		}

		std::lock_guard lock(mutex);
		if (const auto it = originalTypes.find(a_weapon); it != originalTypes.end()) {
			SetType(a_weapon, it->second, "Restore");
		}
	}
}
