// Hook de comprobación de lanzamiento -- ver CastHook.h.

#include "1.- CORE/CastHook.h"

#include "1.- CORE/Forms.h"
#include "3.- WEAPON/WeaponManager.h"

#include <exception>

namespace CastHook
{
	namespace
	{
		// Posición de MagicCaster::CheckCast en la vtable (ActorMagicCaster.h; las entradas propias de VR van al final).
		constexpr std::size_t kCheckCastVtableIndex = 0x0A;

		struct CheckCastHook
		{
			// Primero el original (el juego y los hooks de otros mods); si deja lanzar Lightning Dash
			// y el dash no es posible, se deniega sin lanzar nada (ni efectos ni sonido).
			static bool thunk(RE::ActorMagicCaster* a_this, RE::MagicItem* a_spell, bool a_dualCast, float* a_effectStrength, RE::MagicSystem::CannotCastReason* a_reason, bool a_useBaseValueForCost)
			{
				const bool allowed = func(a_this, a_spell, a_dualCast, a_effectStrength, a_reason, a_useBaseValueForCost);
				if (!allowed || !a_spell || a_spell != Forms::lightningDashSpell) {
					return allowed;
				}

				auto* player = RE::PlayerCharacter::GetSingleton();
				if (!player || a_this->GetCasterAsActor() != player) {
					return allowed;
				}

				// En pausa no se lanza nada: es un menú (magia, favoritos) preguntando si el poder se puede
				// usar para pintarlo en gris. Se deja disponible, sin aviso.
				if (auto* ui = RE::UI::GetSingleton(); ui && ui->GameIsPaused()) {
					return allowed;
				}

				// Ninguna excepción debe cruzar al motor; ante un fallo se deja lanzar.
				try {
					if (Weapon::WeaponManager::GetSingleton()->CanCastLightningDash()) {
						return true;
					}
				} catch (const std::exception& e) {
					logs::error("CastHook: excepción al comprobar Lightning Dash: {}", e.what());
					return allowed;
				} catch (...) {
					logs::error("CastHook: excepción desconocida al comprobar Lightning Dash.");
					return allowed;
				}

				if (a_reason) {
					*a_reason = RE::MagicSystem::CannotCastReason::kCustomReasonNoStart;
				}
				return false;
			}

			static inline REL::Relocation<decltype(thunk)> func;
		};
	}

	void Install()
	{
		REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_ActorMagicCaster[0] };
		CheckCastHook::func = vtbl.write_vfunc(kCheckCastVtableIndex, CheckCastHook::thunk);

		logs::info("CastHook: hook de ActorMagicCaster::CheckCast instalado, Lightning Dash no se lanza si no puede desplazarse.");
	}
}
