// Implementación del sistema de eventos.
// Registra callbacks y procesa eventos recibidos desde Skyrim.

#include "10.- EVENTS/EventManager.h"

#include "2.- INPUT/InputManager.h"
#include "3.- WEAPON/WeaponManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "12.- AUDIO/SoundResolver.h"

#include <optional>

namespace Events
{
	namespace
	{
		// FNV-1a de 32 bits, en tiempo de compilación. No existe ningún
		// registro central de IDs de cosave contra el que verificar
		// unicidad de verdad (no hay forma de "verificarlo" en sentido
		// estricto) — pero derivarlo de una cadena larga y específica de
		// este proyecto (el nombre+autor ya declarados en xmake.lua, no
		// inventados aquí) reduce muchísimo más la probabilidad de
		// colisión por azar con otro plugin que elegir 4 letras cortas a
		// mano, que es justo lo contrario de lo que conviene: cuantas
		// menos combinaciones posibles, más fácil coincidir sin querer.
		constexpr std::uint32_t Fnv1aHash32(std::string_view a_str)
		{
			std::uint32_t hash = 2166136261u;
			for (const char c : a_str) {
				hash ^= static_cast<std::uint8_t>(c);
				hash *= 16777619u;
			}
			return hash;
		}

		// Identificador del bloque de este plugin dentro del cosave (para
		// SetUniqueID) y del registro concreto del ciclo dentro de ese
		// bloque (para OpenRecord) — dos cosas distintas.
		constexpr std::uint32_t kPluginUniqueID = Fnv1aHash32("Capitto91::Throwing Weapons::cosave");
		constexpr std::uint32_t kCycleRecordType = static_cast<std::uint32_t>('CYCL');
		constexpr std::uint32_t kCycleRecordVersion = 1;

		// Datos leídos por SerializationLoadCallback, pendientes de aplicar
		// en kPostLoadGame -- no se toca ningún actor/referencia dentro del
		// propio callback de carga, no hay garantía de que el mundo esté
		// listo ahí todavía (mismo motivo por el que EquipGuard/
		// LoadingScreenWatcher esperan a kDataLoaded, ver CLAUDE.md).
		std::optional<Weapon::WeaponManager::SaveCycleData> g_pendingRecovery;

		void SerializationSaveCallback(SKSE::SerializationInterface* a_intfc)
		{
			const auto data = Weapon::WeaponManager::GetSingleton()->CaptureSaveData();

			if (!a_intfc->OpenRecord(kCycleRecordType, kCycleRecordVersion)) {
				logs::warn("Events::SerializationSaveCallback: no se pudo abrir el registro del cosave.");
				return;
			}

			a_intfc->WriteRecordData(data);
		}

		void SerializationLoadCallback(SKSE::SerializationInterface* a_intfc)
		{
			g_pendingRecovery.reset();

			std::uint32_t type = 0;
			std::uint32_t version = 0;
			std::uint32_t length = 0;
			while (a_intfc->GetNextRecordInfo(type, version, length)) {
				if (type != kCycleRecordType) {
					continue;
				}

				if (version != kCycleRecordVersion) {
					logs::warn("Events::SerializationLoadCallback: versión de registro desconocida ({}), se ignora.", version);
					continue;
				}

				Weapon::WeaponManager::SaveCycleData data{};
				if (a_intfc->ReadRecordData(data) != sizeof(data)) {
					logs::warn("Events::SerializationLoadCallback: registro incompleto, se ignora.");
					continue;
				}

				// Los FormID guardados corresponden a la partida guardada,
				// no necesariamente a esta sesión (el load order pudo
				// cambiar) -- hay que remapearlos, incluso los de
				// referencias dinámicas como la réplica.
				RE::FormID resolved = 0;
				data.weaponFormID = (data.weaponFormID && a_intfc->ResolveFormID(data.weaponFormID, resolved)) ? resolved : 0;
				data.replicaFormID = (data.replicaFormID && a_intfc->ResolveFormID(data.replicaFormID, resolved)) ? resolved : 0;
				data.stuckActorFormID = (data.stuckActorFormID && a_intfc->ResolveFormID(data.stuckActorFormID, resolved)) ? resolved : 0;

				g_pendingRecovery = data;
			}
		}

		void SerializationRevertCallback(SKSE::SerializationInterface*)
		{
			// Partida nueva, o carga de un save sin datos nuestros -- no
			// arrastrar nada de una sesión anterior del proceso.
			g_pendingRecovery.reset();
		}
		// Impide equipar cualquier otra arma mientras la arrojadiza está
		// fuera de la mano (apuntando o lanzada), tal como exige el punto 4
		// de Mecanica del arma.txt. RE::TESEquipEvent se notifica después
		// de que el motor ya ha equipado el objeto (no es cancelable), así
		// que la única forma de bloquearlo es desequiparlo de inmediato al
		// detectarlo.
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
		// Se descartaron dos alternativas, probadas en el juego en la
		// iteración anterior:
		// - TESCellAttachDetachEvent filtrado al jugador: nunca se dispara
		//   para su referencia (igual que en Papyrus, OnCellAttach/
		//   OnCellDetach tampoco lo hacen).
		// - TESCellFullyLoadedEvent: solo salta cuando el motor tiene que
		//   cargar datos nuevos, así que no se dispara al volver a una
		//   celda exterior ya visitada/en caché.
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

		void OnSKSEMessage(SKSE::MessagingInterface::Message* a_message)
		{
			switch (a_message->type) {
			case SKSE::MessagingInterface::kPostLoad:
				// Registro del cosave lo antes posible en el ciclo de vida
				// de SKSE. SKSE::GetSerializationInterface() es un objeto
				// propio de SKSE (disponible ya tras SKSE::Init), no un
				// singleton del motor del juego como RE::UI/
				// RE::ScriptEventSourceHolder -- en teoría no tendría el
				// mismo problema de timing que EquipGuard/
				// LoadingScreenWatcher, pero se registra igualmente en un
				// mensaje en vez de síncronamente en Events::Init(), por
				// consistencia y para no repetir sin comprobar el mismo
				// tipo de suposición que ya falló una vez (ver
				// CHANGELOG.md v1.6.4).
				if (const auto* serialization = SKSE::GetSerializationInterface()) {
					serialization->SetUniqueID(kPluginUniqueID);
					serialization->SetSaveCallback(SerializationSaveCallback);
					serialization->SetLoadCallback(SerializationLoadCallback);
					serialization->SetRevertCallback(SerializationRevertCallback);
					logs::info("Events::OnSKSEMessage: kPostLoad, cosave registrado.");
				} else {
					logs::warn("Events::OnSKSEMessage: kPostLoad, no se pudo obtener SerializationInterface.");
				}
				break;
			case SKSE::MessagingInterface::kInputLoaded:
				// Los dispositivos de entrada ya están listos para
				// registrar event sinks.
				Input::InputManager::GetSingleton()->Init();
				break;
			case SKSE::MessagingInterface::kDataLoaded:
				// Registro de sinks diferido hasta que los datos del
				// juego están cargados (antes, en SKSEPluginLoad, ni
				// siquiera existen los singletons de motor de forma
				// fiable) — mismo motivo que InputManager espera a
				// kInputLoaded, ver CLAUDE.md "Errores comunes a
				// vigilar".
				RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(EquipGuard::GetSingleton());
				RE::UI::GetSingleton()->AddEventSink(LoadingScreenWatcher::GetSingleton());
				Combat::Init();
				// Precarga de los Sound Descriptor del arma (ver
				// 12.- AUDIO/SoundResolver.h): sin esto, el primer
				// lanzamiento de la partida no se oye -- el recurso de
				// audio tarda en cargar de forma asíncrona la primera vez
				// que se solicita (comprobado en el juego).
				Audio::PrecacheAll();
				logs::info("Events::OnSKSEMessage: kDataLoaded, EquipGuard/LoadingScreenWatcher registrados y Combat::Init() ejecutado.");
				break;
			case SKSE::MessagingInterface::kNewGame:
				logs::info("Events::OnSKSEMessage: kNewGame");
				// Partida nueva: nunca hay un ciclo guardado que recuperar.
				Weapon::WeaponManager::GetSingleton()->ResetToInHand();
				break;
			case SKSE::MessagingInterface::kPostLoadGame:
				logs::info("Events::OnSKSEMessage: kPostLoadGame");
				// Si la partida se guardó a mitad de un ciclo,
				// SerializationLoadCallback ya dejó los datos remapeados en
				// g_pendingRecovery -- RecoverOrReset() recupera el arma
				// real en vez de perderle la pista (bug detectado por el
				// usuario: dos copias del arma tras guardar/cargar a
				// medias). Sin datos pendientes (partida antigua, o
				// guardada con el arma ya en mano), se comporta igual que
				// ResetToInHand().
				Weapon::WeaponManager::GetSingleton()->RecoverOrReset(g_pendingRecovery.value_or(Weapon::WeaponManager::SaveCycleData{}));
				g_pendingRecovery.reset();
				break;
			default:
				break;
			}
		}
	}

	void Init()
	{
		// Solo el registro del propio listener de mensajería aquí — los
		// sinks de motor (EquipGuard/LoadingScreenWatcher) y Combat::Init()
		// se difieren a kDataLoaded (ver OnSKSEMessage): en el momento en
		// que se llama a Init() (desde SKSEPluginLoad) todavía no hay
		// garantía de que los singletons del motor existan de forma
		// fiable.
		SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
		logs::info("EventManager inicializado (a la espera de kInputLoaded/kDataLoaded).");
	}
}
