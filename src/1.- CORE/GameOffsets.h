// Funciones nativas del motor no expuestas por commonlibsse-ng (IDs de Address Library).

#pragma once

namespace GameOffsets
{
	// Actor::CombatHit: golpe cuerpo a cuerpo de a_attacker sobre a_target (solo SE/AE).
	// ID tomado de Precision; comprobar REL::Module::IsVR() antes de llamar.
	using tDealDamage = void*(__fastcall*)(RE::Actor* a_attacker, RE::Actor* a_target, RE::Projectile* a_sourceProjectile, bool a_bLeftHand);
	inline REL::Relocation<tDealDamage> DealDamage{ REL::RelocationID(37673, 38627) };

	// Función nativa que procesa un golpe (daño, reacciones, IA, muerte): void(Actor*, HitData&).
	// Se lee del call E8 dentro de CombatHit; nullptr si no es E8 o en VR. La usa Combat::ApplyWeaponHit.
	using tProcessHit = void (*)(RE::Actor* a_victim, RE::HitData& a_hitData);

	inline tProcessHit ResolveProcessHit()
	{
		if (REL::Module::IsVR()) {
			return nullptr;
		}

		const REL::Relocation<std::uintptr_t> callSite{ REL::RelocationID(37673, 38627), REL::VariantOffset(0x3C0, 0x4A8, 0) };
		const auto*                           bytes = reinterpret_cast<const std::uint8_t*>(callSite.address());
		if (bytes[0] != 0xE8) {
			return nullptr;
		}

		std::int32_t rel32 = 0;
		std::memcpy(&rel32, bytes + 1, sizeof(rel32));
		return reinterpret_cast<tProcessHit>(callSite.address() + 5 + rel32);
	}
}
