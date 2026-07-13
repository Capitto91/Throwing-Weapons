// Implementación del sistema de eventos.
// Registra callbacks y procesa eventos recibidos desde Skyrim.

#include "10.- EVENTS/EventManager.h"

#include "2.- INPUT/InputManager.h"
#include "3.- WEAPON/WeaponManager.h"

namespace Events
{
	namespace
	{
		// Impide equipar cualquier otra arma mientras la arrojadiza está
		// fuera de la mano (apuntando, lanzada, clavada o regresando), tal
		// como exige el punto 4 de Mecanica del arma.txt. RE::TESEquipEvent
		// se notifica después de que el motor ya ha equipado el objeto (no
		// es cancelable), así que la única forma de bloquearlo es
		// desequiparlo de inmediato al detectarlo.
		class EquipGuard final : public RE::BSTEventSink<RE::TESEquipEvent>
		{
		public:
			static EquipGuard* GetSingleton()
			{
				static EquipGuard singleton;
				return &singleton;
			}

			EquipGuard(const EquipGuard&) = delete;
			EquipGuard(EquipGuard&&) = delete;
			EquipGuard& operator=(const EquipGuard&) = delete;
			EquipGuard& operator=(EquipGuard&&) = delete;

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!a_event || !a_event->equipped || !player || a_event->actor.get() != player) {
					return RE::BSEventNotifyControl::kContinue;
				}

				if (Weapon::WeaponManager::GetSingleton()->GetState() == Weapon::State::kInHand) {
					return RE::BSEventNotifyControl::kContinue;
				}

				auto* form = RE::TESForm::LookupByID(a_event->baseObject);
				if (auto* boundObject = form ? form->As<RE::TESBoundObject>() : nullptr) {
					RE::ActorEquipManager::GetSingleton()->UnequipObject(player, boundObject);
				}

				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			EquipGuard() = default;
			~EquipGuard() override = default;
		};

		// Recupera el arma si el ciclo estaba en marcha cuando se cierra
		// cualquier pantalla de carga (puerta, viaje rápido...). A
		// diferencia de la resincronización de kPostLoadGame, aquí sí es
		// seguro reequipar: WeaponManager recuerda el arma exacta de la
		// sesión en curso, no hace falta adivinar nada (ver
		// WeaponManager::OnLoadingScreenClosed).
		//
		// Se descartaron dos alternativas, probadas en el juego:
		// - TESCellAttachDetachEvent filtrado al jugador: nunca se dispara
		//   para su referencia (igual que en Papyrus, OnCellAttach/
		//   OnCellDetach tampoco lo hacen).
		// - TESCellFullyLoadedEvent: solo salta cuando el motor tiene que
		//   cargar datos nuevos, así que no se dispara al volver a una
		//   celda exterior ya visitada/en caché (p. ej. salir de un
		//   interior hacia el exterior del que se venía).
		// El cierre de "Loading Menu" es independiente de la caché: salta
		// siempre que el jugador termina cualquier transición con pantalla
		// de carga.
		class LoadingScreenWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			static LoadingScreenWatcher* GetSingleton()
			{
				static LoadingScreenWatcher singleton;
				return &singleton;
			}

			LoadingScreenWatcher(const LoadingScreenWatcher&) = delete;
			LoadingScreenWatcher(LoadingScreenWatcher&&) = delete;
			LoadingScreenWatcher& operator=(const LoadingScreenWatcher&) = delete;
			LoadingScreenWatcher& operator=(LoadingScreenWatcher&&) = delete;

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && !a_event->opening && a_event->menuName == RE::LoadingMenu::MENU_NAME) {
					Weapon::WeaponManager::GetSingleton()->OnLoadingScreenClosed();
				}

				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			LoadingScreenWatcher() = default;
			~LoadingScreenWatcher() override = default;
		};

		// El golpe contra un actor no se refleja de forma fiable en
		// ImpactResult (ver Throw::TrackProjectile — comprobado en el
		// juego: al clavarse en un enemigo, el sondeo nunca detectaba el
		// impacto), así que se detecta aparte con RE::TESHitEvent.
		// Se filtra por a_event->source (el arma causante), no por
		// a_event->projectile: comprobado en el juego que este último llega
		// a 0 al lanzar vía LaunchArrow con un arma como a_weap en vez de un
		// TESObjectWEAP real de un arco, mientras que source sí trae el
		// FormID del arma correctamente.
		class ProjectileHitWatcher final : public RE::BSTEventSink<RE::TESHitEvent>
		{
		public:
			static ProjectileHitWatcher* GetSingleton()
			{
				static ProjectileHitWatcher singleton;
				return &singleton;
			}

			ProjectileHitWatcher(const ProjectileHitWatcher&) = delete;
			ProjectileHitWatcher(ProjectileHitWatcher&&) = delete;
			ProjectileHitWatcher& operator=(const ProjectileHitWatcher&) = delete;
			ProjectileHitWatcher& operator=(ProjectileHitWatcher&&) = delete;

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESHitEvent* a_event, RE::BSTEventSource<RE::TESHitEvent>*) override
			{
				auto* weaponManager = Weapon::WeaponManager::GetSingleton();
				if (weaponManager->GetState() != Weapon::State::kThrown) {
					return RE::BSEventNotifyControl::kContinue;
				}

				auto* activeWeapon = weaponManager->GetActiveWeapon();
				if (a_event && activeWeapon && a_event->source == activeWeapon->GetFormID()) {
					weaponManager->OnProjectileImpact();
				}

				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			ProjectileHitWatcher() = default;
			~ProjectileHitWatcher() override = default;
		};

		void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
		{
			switch (a_message->type) {
			case SKSE::MessagingInterface::kInputLoaded:
				// Los dispositivos de entrada ya están listos para
				// registrar event sinks.
				Input::InputManager::GetSingleton()->Init();
				break;
			case SKSE::MessagingInterface::kNewGame:
			case SKSE::MessagingInterface::kPostLoadGame:
				// El estado del arma vive solo en memoria (no en el save):
				// al cargar o empezar partida lo resincronizamos con la
				// mano del jugador en vez de arrastrar un estado obsoleto de
				// una sesión anterior del proceso.
				Weapon::WeaponManager::GetSingleton()->ResetToInHand();
				break;
			default:
				break;
			}
		}
	}

	void Init()
	{
		SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(EquipGuard::GetSingleton());
		RE::UI::GetSingleton()->AddEventSink(LoadingScreenWatcher::GetSingleton());
		RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(ProjectileHitWatcher::GetSingleton());
	}
}