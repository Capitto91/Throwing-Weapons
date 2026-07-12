// Implementación del sistema de lanzamiento.
// Controla la creación, activación y seguimiento del arma lanzada.

#include "4.- THROW/ThrowManager.h"

#include "1.- CORE/Constants.h"

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
        RE::Projectile::ProjectileRot GetCameraAimAngles()
        {
            auto*        camera = RE::PlayerCamera::GetSingleton();
            RE::NiPoint3 eulerAngles;

            if (camera && camera->cameraRoot) {
                camera->cameraRoot->world.rotate.ToEulerAnglesXYZ(eulerAngles);
            }

            return { eulerAngles.x, eulerAngles.z };
        }
    }

    void LaunchWeapon(RE::Actor* a_shooter)
    {
        if (!a_shooter) {
            return;
        }

        auto* projectileBase = RE::TESForm::LookupByEditorID<RE::BGSProjectile>(Constants::kThrowableProjectile);
        if (!projectileBase) {
            logs::error(
                "No se encontró el Projectile \"{}\": revisa que exista en la Creation Kit.",
                Constants::kThrowableProjectile);
            return;
        }

        RE::Projectile::LaunchData launchData(projectileBase, a_shooter, GetLaunchOrigin(a_shooter), GetCameraAimAngles());
        launchData.useOrigin = true;
        // Apuntado manual por cámara, sin ayuda de autoapuntado del motor.
        launchData.autoAim = false;

        RE::ProjectileHandle handle;
        RE::Projectile::Launch(&handle, launchData);
    }
}