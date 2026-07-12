// Gestiona todo el proceso de lanzamiento.
// Crea el proyectil réplica del arma, calcula posición inicial y aplica la fuerza
// inicial del lanzamiento.

#pragma once

namespace Throw
{
    // Lanza el proyectil réplica del arma desde la mano derecha del actor,
    // hacia donde apunta la cámara. No hay mecánica de carga (Mecanica del
    // arma.txt, punto 3): la fuerza del lanzamiento la determina por
    // completo el formulario Projectile configurado en la Creation Kit, no
    // algo que calculemos aquí.
    void LaunchWeapon(RE::Actor* a_shooter);
}