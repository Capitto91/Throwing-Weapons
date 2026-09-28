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
		// Tipo original de cada arma cambiada. Compartido con los hilos de animación: mutex.
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
		void SetType(RE::TESObjectWEAP* a_weapon, RE::WEAPON_TYPE a_type)
		{
			if (a_weapon->GetWeaponType() == a_type) {
				return;
			}

			a_weapon->weaponData.animationType = a_type;
		}

		// Requiere el mutex tomado.
		void RestoreAllExcept(RE::TESObjectWEAP* a_keep)
		{
			for (auto& [weapon, original] : originalTypes) {
				if (weapon != a_keep) {
					SetType(weapon, original);
				}
			}
		}

		// Maza solo desenvainada del todo y con el ciclo en reposo (kInHand).
		bool WantsDrawnType(RE::Actor& a_actor)
		{
			return a_actor.AsActorState()->GetWeaponState() == RE::WEAPON_STATE::kDrawn &&
			       Weapon::WeaponManager::GetSingleton()->GetState() == Weapon::State::kInHand;
		}

		// ¿Cuelga ya el modelo del hueso "WEAPON" de la mano?
		bool IsModelInHand(RE::Actor& a_actor)
		{
			auto* root = a_actor.Get3D(false);
			auto* weaponBone = root ? root->GetObjectByName("WEAPON") : nullptr;
			auto* asNode = weaponBone ? weaponBone->AsNode() : nullptr;
			return asNode && !asNode->GetChildren().empty();
		}

		void Promote();

		// Programa Promote en el hilo principal. Requiere el mutex tomado.
		void RequestPromote()
		{
			promoteToken = Scheduler::After(Constants::kDrawnTypePromoteRetryInterval, Promote);
		}

		// Paso a maza en el hilo principal, solo con el modelo ya en la mano;
		// reintenta cada kDrawnTypePromoteRetryInterval.
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

			SetType(weapon, Constants::kDrawnAnimationWeaponType);
		}

		void Sync(RE::Actor& a_actor)
		{
			auto* weapon = GetEquippedThrowableWeapon(a_actor);

			std::lock_guard lock(mutex);

			// Sin el arma en la mano ninguna se queda como maza.
			RestoreAllExcept(weapon);
			if (!weapon) {
				return;
			}

			const auto original = originalTypes.try_emplace(weapon, weapon->GetWeaponType()).first->second;
			if (!WantsDrawnType(a_actor)) {
				Scheduler::Cancel(promoteToken);
				promoteToken.reset();
				SetType(weapon, original);
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
				// Solo el jugador.
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (a_event && player && a_event->holder == player) {
					Sync(*player);
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		Sink sink;
	}

	void EnsureRegistered(RE::Actor& a_actor)
	{
		// Todos los grafos (tercera y primera persona); AddEventSink ignora repetidos.
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

		Sync(a_actor);
	}

	void Restore(RE::TESObjectWEAP* a_weapon)
	{
		if (!a_weapon) {
			return;
		}

		std::lock_guard lock(mutex);
		if (const auto it = originalTypes.find(a_weapon); it != originalTypes.end()) {
			SetType(a_weapon, it->second);
		}
	}
}
