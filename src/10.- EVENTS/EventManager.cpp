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
    }
}