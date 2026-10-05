// Mensajes de SKSE, sinks del motor y cosave -- ver EventManager.h.

#include "10.- EVENTS/EventManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Requirements.h"
#include "10.- EVENTS/OARFunctions.h"
#include "11.- SKYRIM/TDMBridge.h"
#include "12.- AUDIO/SoundResolver.h"
#include "14.- UI/ConfigMenu.h"
#include "2.- INPUT/InputManager.h"
#include "3.- WEAPON/LightningDash.h"
#include "3.- WEAPON/WeaponManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/GlowMapControl.h"
#include "8.- ANIMATION/PowerAttackVFX.h"

#include <optional>

namespace Events
{
	namespace
	{
		// FNV-1a de 32 bits en compilación, para el ID del cosave.
		constexpr std::uint32_t Fnv1aHash32(std::string_view a_str)
		{
			std::uint32_t hash = 2166136261u;
			for (const char c : a_str) {
				hash ^= static_cast<std::uint8_t>(c);
				hash *= 16777619u;
			}
			return hash;
		}

		// ID del plugin en el cosave y del registro del ciclo.
		constexpr std::uint32_t kPluginUniqueID = Fnv1aHash32("Capitto91::ThorMjolnir::cosave");
		constexpr std::uint32_t kCycleRecordType = static_cast<std::uint32_t>('CYCL');
		constexpr std::uint32_t kCycleRecordVersion = 1;

		// Datos del cosave pendientes de aplicar en kPostLoadGame.
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

				// Remapea los FormID guardados al orden de carga actual.
				RE::FormID resolved = 0;
				data.weaponFormID = (data.weaponFormID && a_intfc->ResolveFormID(data.weaponFormID, resolved)) ? resolved : 0;
				data.replicaFormID = (data.replicaFormID && a_intfc->ResolveFormID(data.replicaFormID, resolved)) ? resolved : 0;
				data.stuckActorFormID = (data.stuckActorFormID && a_intfc->ResolveFormID(data.stuckActorFormID, resolved)) ? resolved : 0;

				g_pendingRecovery = data;
			}
		}

		void SerializationRevertCallback(SKSE::SerializationInterface*)
		{
			// Partida nueva o sin datos nuestros: se descarta lo anterior.
			g_pendingRecovery.reset();
		}
		// Desequipa cualquier otra arma mientras la arrojadiza está fuera de la mano.
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

		// Concede o retira el poder Lightning Dash al equipar/desequipar el arma arrojadiza.
		// Avisa a WeaponManager::OnThrowableWeaponEquipChanged.
		class LightningDashWatcher final : public RE::BSTEventSink<RE::TESEquipEvent>
		{
		public:
			static LightningDashWatcher* GetSingleton()
			{
				static LightningDashWatcher singleton;
				return &singleton;
			}

			LightningDashWatcher(const LightningDashWatcher&) = delete;
			LightningDashWatcher(LightningDashWatcher&&) = delete;
			LightningDashWatcher& operator=(const LightningDashWatcher&) = delete;
			LightningDashWatcher& operator=(LightningDashWatcher&&) = delete;

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!a_event || !player || a_event->actor.get() != player) {
					return RE::BSEventNotifyControl::kContinue;
				}

				// Al equipar el arma arrojadiza, efectos de ataque fuerte y brillo, también durante una carga.
				{
					auto* equipForm = RE::TESForm::LookupByID(a_event->baseObject);
					auto* equipWeapon = equipForm ? equipForm->As<RE::TESObjectWEAP>() : nullptr;
					if (a_event->equipped && equipWeapon && equipWeapon->HasKeywordString(Constants::kThrowableWeaponKeyword)) {
						Animation::PowerAttackVFX::EnsureRegistered(*player);
						Animation::GlowMapControl::EnsureRunning();
					}
				}

				// Durante una pantalla de carga se ignora; kPostLoadGame ya concede el poder.
				if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
					return RE::BSEventNotifyControl::kContinue;
				}

				auto* form = RE::TESForm::LookupByID(a_event->baseObject);
				auto* weapon = form ? form->As<RE::TESObjectWEAP>() : nullptr;
				if (!weapon || !weapon->HasKeywordString(Constants::kThrowableWeaponKeyword)) {
					return RE::BSEventNotifyControl::kContinue;
				}

				Weapon::WeaponManager::GetSingleton()->OnThrowableWeaponEquipChanged(a_event->equipped);

				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			LightningDashWatcher() = default;
			~LightningDashWatcher() override = default;
		};

		// El jugador lanza el poder Lightning Dash: encola WeaponManager::OnLightningDashCast
		// para ejecutarlo fuera del procesado del hechizo.
		class LightningDashCastWatcher final : public RE::BSTEventSink<RE::TESSpellCastEvent>
		{
		public:
			static LightningDashCastWatcher* GetSingleton()
			{
				static LightningDashCastWatcher singleton;
				return &singleton;
			}

			LightningDashCastWatcher(const LightningDashCastWatcher&) = delete;
			LightningDashCastWatcher(LightningDashCastWatcher&&) = delete;
			LightningDashCastWatcher& operator=(const LightningDashCastWatcher&) = delete;
			LightningDashCastWatcher& operator=(LightningDashCastWatcher&&) = delete;

		protected:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* a_event, RE::BSTEventSource<RE::TESSpellCastEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				auto* spell = Weapon::LightningDash::GetSpell();
				if (!a_event || !player || !spell || a_event->object.get() != player || a_event->spell != spell->GetFormID()) {
					return RE::BSEventNotifyControl::kContinue;
				}

				SKSE::GetTaskInterface()->AddTask([]() {
					Weapon::WeaponManager::GetSingleton()->OnLightningDashCast();
				});

				return RE::BSEventNotifyControl::kContinue;
			}

		private:
			LightningDashCastWatcher() = default;
			~LightningDashCastWatcher() override = default;
		};

		// Al cerrarse cualquier pantalla de carga, avisa a WeaponManager::OnLoadingScreenClosed.
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
					if (auto* player = RE::PlayerCharacter::GetSingleton()) {
						Animation::PowerAttackVFX::EnsureRegistered(*player);
					}
					// Con coc desde el menú principal no llega kNewGame ni kPostLoadGame.
					Animation::GlowMapControl::EnsureRunning();

					// Efectos persistentes del VisualEffect del poder guardados en la partida.
					SKSE::GetTaskInterface()->AddTask([]() {
						Weapon::LightningDash::RemoveLegacyEffects();
					});
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
				// Registra los callbacks del cosave.
				if (const auto* serialization = SKSE::GetSerializationInterface()) {
					serialization->SetUniqueID(kPluginUniqueID);
					serialization->SetSaveCallback(SerializationSaveCallback);
					serialization->SetLoadCallback(SerializationLoadCallback);
					serialization->SetRevertCallback(SerializationRevertCallback);
				} else {
					logs::warn("Events::OnSKSEMessage: kPostLoad, no se pudo obtener SerializationInterface.");
				}

				// Registra las funciones de OAR (aquí o antes).
				OARFunctions::RegisterAll();

				// Pide la API de TDM.
				TDMBridge::Init();

				// Menú de configuración (aquí, no antes).
				UI::ConfigMenu::Register();
				// Todos los plugins SKSE ya están cargados: requisitos al log.
				Requirements::CheckPlugins();
				break;
			case SKSE::MessagingInterface::kInputLoaded:
				// Sink de entrada.
				Input::InputManager::GetSingleton()->Init();
				break;
			case SKSE::MessagingInterface::kDataLoaded:
				// Sinks del motor, con los datos del juego ya cargados.
				RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(EquipGuard::GetSingleton());
				RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(LightningDashWatcher::GetSingleton());
				RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink(LightningDashCastWatcher::GetSingleton());
				RE::UI::GetSingleton()->AddEventSink(LoadingScreenWatcher::GetSingleton());
				Combat::Init();
				Requirements::CheckPluginFile();
				// Gasta el primer uso de cada sonido del arma.
				Audio::WarmUpAll();
				break;
			case SKSE::MessagingInterface::kNewGame:
				logs::info("Partida nueva.");
				// Partida nueva: no hay ciclo guardado.
				Weapon::WeaponManager::GetSingleton()->ResetToInHand();
				Animation::GlowMapControl::EnsureRunning();
				break;
			case SKSE::MessagingInterface::kPostLoadGame:
				logs::info("Partida cargada.");
				// Recupera el ciclo guardado en el cosave, o deja el arma en mano.
				Weapon::WeaponManager::GetSingleton()->RecoverOrReset(g_pendingRecovery.value_or(Weapon::WeaponManager::SaveCycleData{}));
				// Concede Lightning Dash si el arma ya está en la mano.
				Weapon::WeaponManager::GetSingleton()->RestoreLightningDashPower();
				// Quita los efectos persistentes que su VisualEffect dejó guardados en la partida.
				Weapon::LightningDash::RemoveLegacyEffects();
				if (auto* player = RE::PlayerCharacter::GetSingleton()) {
					Animation::PowerAttackVFX::EnsureRegistered(*player);
				}
				Animation::GlowMapControl::EnsureRunning();
				g_pendingRecovery.reset();
				break;
			default:
				break;
			}
		}
	}

	void Init()
	{
		// Solo el listener de mensajería; lo demás espera a sus mensajes.
		SKSE::GetMessagingInterface()->RegisterListener(OnSKSEMessage);
	}
}
