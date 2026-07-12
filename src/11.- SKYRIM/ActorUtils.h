// Contiene funciones auxiliares relacionadas con actores de Skyrim.
// Facilita comprobaciones de enemigos, distancias y selección de objetivos.

#pragma once

namespace ActorUtils
{
    // Devuelve true si el actor tiene equipada en la mano derecha (mano
    // principal) el arma arrojadiza única (identificada por su Keyword, ver
    // Constants::kThrowableWeaponKeyword). La mano izquierda no participa en
    // esta mecánica.
    bool IsThrowableWeaponEquipped(RE::Actor* a_actor);
}