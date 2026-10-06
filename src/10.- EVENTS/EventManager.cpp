// Mensajes de SKSE, sinks del motor y cosave -- ver EventManager.h.

#include "10.- EVENTS/EventManager.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/Forms.h"
#include "1.- CORE/Requirements.h"
#include "10.- EVENTS/OARFunctions.h"
#include "11.- SKYRIM/ActorUtils.h"
#include "11.- SKYRIM/TDMBridge.h"
#include "12.- AUDIO/SoundResolver.h"
#include "14.- UI/ConfigMenu.h"
#include "2.- INPUT/InputManager.h"
#include "3.- WEAPON/LightningDash.h"
#include "3.- WEAPON/WeaponManager.h"
#include "7.- COMBAT/DamageManager.h"
#include "8.- ANIMATION/GlowMapControl.h"
#include "8.- ANIMATION/PowerAttackVFX.h"
#include "8.- ANIMATION/WeaponGlow.h"

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

		// Con el ciclo en marcha la mano derecha queda libre para los puños: si a_form, recién equipado, está en ella, se
		// deshace en el acto (arma: de esa mano; hechizo o pergamino: DeselectSpell). La izquierda y lo demás, libres.
		// Dentro del propio evento: hacerlo después, con un menú abierto, lo hace caer (lee su lista de objetos ya vieja).
		void KeepRightHandFree(RE::PlayerCharacter& a_player, RE::TESForm* a_form)
		{
			if (!a_form) {
				return;
			}

			const auto* left = a_player.GetEquippedObject(true);
			const auto* right = a_player.GetEquippedObject(false);
			logs::info("Events::EquipWatcher: '{}' equipado con el ciclo en marcha -- izquierda '{}', derecha '{}'.", a_form->GetName(), left ? left->GetName() : "nada", right ? right->GetName() : "nada");
			if (right != a_form) {
				return;
			}

			if (auto* spell = a_form->As<RE::SpellItem>()) {
				a_player.DeselectSpell(spell);
			} else if (auto* boundObject = a_form->As<RE::TESBoundObject>()) {
				// Solo de la derecha si el mismo objeto está también en la izquierda; si no, como siempre.
				const auto* slot = left == a_form ? Forms::rightHandEquipSlot : nullptr;
				RE::ActorEquipManager::GetSingleton()->UnequipObject(&a_player, boundObject, nullptr, 1, slot);
			}
			logs::info("Events::EquipWatcher: '{}' quitado de la mano derecha.", a_form->GetName());
		}

		// Equipado del jugador. Con el ciclo en marcha, mantiene libre la mano derecha (KeepRightHandFree); al equipar o
		// desequipar el arma arrojadiza, efectos de power attack y glow, y avisa a OnThrowableWeaponEquipChanged.
		class EquipWatcher final : public RE::BSTEventSink<RE::TESEquipEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* a_event, RE::BSTEventSource<RE::TESEquipEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!a_event || !player || a_event->actor.get() != player) {
					return RE::BSEventNotifyControl::kContinue;
				}

				auto* form = RE::TESForm::LookupByID(a_event->baseObject);

				if (a_event->equipped && Weapon::WeaponManager::GetSingleton()->GetState() != Weapon::State::kInHand) {
					KeepRightHandFree(*player, form);
				}

				if (!ActorUtils::IsThrowableWeapon(form)) {
					return RE::BSEventNotifyControl::kContinue;
				}

				// Al equipar el arma arrojadiza, efectos de ataque fuerte y brillo, también durante una carga.
				if (a_event->equipped) {
					Animation::PowerAttackVFX::EnsureRegistered(*player);
					Animation::GlowMapControl::EnsureRunning();
				}

				// Durante una pantalla de carga se ignora; kPostLoadGame ya concede el poder.
				if (auto* ui = RE::UI::GetSingleton(); ui && ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME)) {
					return RE::BSEventNotifyControl::kContinue;
				}

				Weapon::WeaponManager::GetSingleton()->OnThrowableWeaponEquipChanged(a_event->equipped);

				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// El jugador lanza el poder Lightning Dash: encola WeaponManager::OnLightningDashCast
		// para ejecutarlo fuera del procesado del hechizo.
		class LightningDashCastWatcher final : public RE::BSTEventSink<RE::TESSpellCastEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* a_event, RE::BSTEventSource<RE::TESSpellCastEvent>*) override
			{
				auto* player = RE::PlayerCharacter::GetSingleton();
				auto* spell = Forms::lightningDashSpell;
				if (!a_event || !player || !spell || a_event->object.get() != player || a_event->spell != spell->GetFormID()) {
					return RE::BSEventNotifyControl::kContinue;
				}

				SKSE::GetTaskInterface()->AddTask([]() {
					Weapon::WeaponManager::GetSingleton()->OnLightningDashCast();
				});

				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Al cerrarse cualquier pantalla de carga, avisa a WeaponManager::OnLoadingScreenClosed.
		class LoadingScreenWatcher final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				if (a_event && !a_event->opening && a_event->menuName == RE::LoadingMenu::MENU_NAME) {
					Weapon::WeaponManager::GetSingleton()->OnLoadingScreenClosed();
					if (auto* player = RE::PlayerCharacter::GetSingleton()) {
						Animation::PowerAttackVFX::EnsureRegistered(*player);
					}
					// Con coc desde el menú principal no llega kNewGame ni kPostLoadGame.
					Animation::GlowMapControl::EnsureRunning();

					// Restos guardados en la partida: efectos del VisualEffect antiguo del poder y destellos de un ciclo a medias.
					SKSE::GetTaskInterface()->AddTask([]() {
						Weapon::LightningDash::RemoveLegacyEffects();
						Animation::RemoveStrayWeaponGlows();
					});
				}

				return RE::BSEventNotifyControl::kContinue;
			}
		};

		// Sinks registrados en kDataLoaded; viven toda la sesión.
		EquipWatcher             g_equipWatcher;
		LightningDashCastWatcher g_lightningDashCastWatcher;
		LoadingScreenWatcher     g_loadingScreenWatcher;

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
				// Formularios del plugin, antes que todo lo que los usa (sinks y Requirements::CheckPluginFile).
				Forms::Load();
				// Sinks del motor, con los datos del juego ya cargados.
				RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESEquipEvent>(&g_equipWatcher);
				RE::ScriptEventSourceHolder::GetSingleton()->AddEventSink<RE::TESSpellCastEvent>(&g_lightningDashCastWatcher);
				RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(&g_loadingScreenWatcher);
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
