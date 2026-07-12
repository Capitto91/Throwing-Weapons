// Implementación del sistema de lanzamiento.
// Controla la creación, activación y seguimiento del arma lanzada.

#include "4.- THROW/ThrowManager.h"

#include "1.- CORE/Constants.h"

#include <algorithm>
#include <cmath>

namespace Throw
{
    namespace
    {
        // Punto de origen del lanzamiento: el nodo del arma en la mano
        // derecha, para que el proyectil aparezca en la misma posición que
        // el arma (Mecanica del arma.txt, punto 2), no en la cámara.
        RE::NiPoint3 GetLaunchOrigin(RE::Actor* a_shooter)
        {
            if (auto* weaponNode = a_shooter->GetNodeByName("WEAPON")) {
                return weaponNode->world.translate;
            }

            return a_shooter->GetPosition();
        }

        // Ángulos de lanzamiento a partir de hacia dónde apunta la cámara.
        // Se calculan a partir del vector de dirección (columna Y de la
        // matriz, "adelante" en la convención de Gamebryo/NetImmerse), no
        // con ToEulerAnglesXYZ: esa función descompone en orden X-Y-Z, pero
        // Skyrim compone estas rotaciones en orden Z-X-Y (por eso solo hay
        // EulerAnglesToAxesZXY, no una versión XYZ, para construir
        // matrices); con solo giro horizontal coincide casi por casualidad,
        // pero se descuadra en cuanto se mezcla con inclinación vertical.
        RE::Projectile::ProjectileRot GetCameraAimAngles()
        {
            auto* camera = RE::PlayerCamera::GetSingleton();
            if (!camera || !camera->cameraRoot) {
                return { 0.0f, 0.0f };
            }

            const RE::NiPoint3 forward = camera->cameraRoot->world.rotate.GetVectorY();
            const float        pitch = -std::asin(std::clamp(forward.z, -1.0f, 1.0f));
            const float        yaw = std::atan2(forward.x, forward.y);

            return { pitch, yaw };
        }
    }

    void LaunchWeapon(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon)
    {
        if (!a_shooter) {
            return;
        }

        auto* ammo = RE::TESForm::LookupByEditorID<RE::TESAmmo>(Constants::kThrowableAmmo);
        if (!ammo) {
            logs::error(
                "No se encontró el Ammo \"{}\": revisa que exista en la Creation Kit.",
                Constants::kThrowableAmmo);
            return;
        }

        const auto origin = GetLaunchOrigin(a_shooter);
        const auto angles = GetCameraAimAngles();

        RE::ProjectileHandle handle;
        RE::Projectile::LaunchArrow(&handle, a_shooter, ammo, a_weapon, origin, angles);
    }
}
