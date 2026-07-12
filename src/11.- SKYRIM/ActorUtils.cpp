// Implementación de utilidades para actores.
// Centraliza operaciones repetidas sobre NPCs y criaturas.

#include "11.- SKYRIM/ActorUtils.h"

#include "1.- CORE/Constants.h"

namespace ActorUtils
{
    bool IsThrowableWeaponEquipped(RE::Actor* a_actor)
    {
        if (!a_actor) {
            return false;
        }

        // false = mano derecha / mano principal.
        const auto* rightHand = a_actor->GetEquippedObject(false);
        const auto* weapon = rightHand ? rightHand->As<RE::TESObjectWEAP>() : nullptr;

        return weapon && weapon->HasKeywordString(Constants::kThrowableWeaponKeyword);
    }
}