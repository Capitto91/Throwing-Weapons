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

		// Busca recursivamente, a partir de a_node, un descendiente cuyo
		// nombre sea Constants::kEmbeddedWeaponNodeName y lo desengancha de
		// su padre en cuanto lo encuentra.
		bool DetachNodeByName(RE::NiNode* a_node, std::string_view a_name)
		{
			if (!a_node) {
				return false;
			}

			for (auto& child : a_node->GetChildren()) {
				auto* childPtr = child.get();
				if (!childPtr) {
					continue;
				}

				if (childPtr->name == a_name) {
					a_node->DetachChild(childPtr);
					return true;
				}

				if (DetachNodeByName(childPtr->AsNode(), a_name)) {
					return true;
				}
			}

			return false;
		}
	}

	RE::ProjectileHandle LaunchWeapon(RE::Actor* a_shooter, RE::TESObjectWEAP* a_weapon)
	{
		if (!a_shooter) {
			return {};
		}

		auto* ammo = RE::TESForm::LookupByEditorID<RE::TESAmmo>(Constants::kThrowableAmmo);
		if (!ammo) {
			logs::error(
				"No se encontró el Ammo \"{}\": revisa que exista en la Creation Kit.",
				Constants::kThrowableAmmo);
			return {};
		}

		const auto origin = GetLaunchOrigin(a_shooter);
		const auto angles = GetCameraAimAngles();

		RE::ProjectileHandle handle;
		RE::Projectile::LaunchArrow(&handle, a_shooter, ammo, a_weapon, origin, angles);
		return handle;
	}

	void DetachEmbeddedWeapon(RE::TESObjectREFR* a_target)
	{
		if (!a_target) {
			return;
		}

		auto* root = a_target->Get3D();
		if (DetachNodeByName(root ? root->AsNode() : nullptr, Constants::kEmbeddedWeaponNodeName)) {
			logs::info("DetachEmbeddedWeapon: nodo \"{}\" desenganchado de \"{}\"", Constants::kEmbeddedWeaponNodeName, a_target->GetName());
		} else {
			logs::warn(
				"DetachEmbeddedWeapon: no se encontró el nodo \"{}\" en \"{}\" (¿se llamó demasiado pronto tras el impacto?)",
				Constants::kEmbeddedWeaponNodeName,
				a_target->GetName());
		}
	}
}
