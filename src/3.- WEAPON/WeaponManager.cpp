// Implementación del ciclo de vida del arma.
// Coordina la transición entre arma en mano, arma lanzada y arma recuperada.

#include "3.- WEAPON/WeaponManager.h"

#include "4.- THROW/ThrowManager.h"
#include "4.- THROW/ThrowProjectile.h"

namespace Weapon
{
	WeaponManager* WeaponManager::GetSingleton()
	{
		static WeaponManager singleton;
		return &singleton;
	}

	void WeaponManager::OnAimButtonDown()
	{
		switch (weaponState.GetState()) {
		case State::kInHand:
			BeginAiming();
			break;
		case State::kAiming:
			// Solo se puede recibir una pulsación nueva estando ya
			// "apuntando" si nos perdimos el botón de soltar anterior (p.ej.
			// una pantalla de carga a mitad de la pulsación): no hay nada
			// que deshacer todavía, así que reiniciamos el ciclo con la
			// pulsación actual en vez de quedarnos atascados.
			weaponState.SetState(State::kInHand);
			BeginAiming();
			break;
		case State::kThrown:
		case State::kStuck:
			RecallWeapon();
			break;
		default:
			break;
		}
	}

	void WeaponManager::OnAimButtonUp()
	{
		if (weaponState.GetState() == State::kAiming) {
			ThrowWeapon();
		}
	}

	void WeaponManager::ResetToInHand()
	{
		weaponState.SetActiveWeapon(nullptr);
		weaponState.SetProjectileHandle({});
		weaponState.SetState(State::kInHand);
	}

	void WeaponManager::OnProjectileImpact()
	{
		if (weaponState.GetState() == State::kThrown) {
			weaponState.SetState(State::kStuck);
		}
	}

	void WeaponManager::OnProjectileMaxRangeReached()
	{
		if (weaponState.GetState() == State::kThrown) {
			RecallWeapon();
		}
	}

	void WeaponManager::OnProjectileEnteredWater()
	{
		if (weaponState.GetState() == State::kThrown) {
			RecallWeapon();
		}
	}

	void WeaponManager::OnLoadingScreenClosed()
	{
		switch (weaponState.GetState()) {
		case State::kThrown:
		case State::kStuck:
		case State::kReturning:
			RecallWeapon();
			break;
		case State::kAiming:
			// El arma sigue en la mano (el desequipar solo pasa al soltar);
			// solo reordenamos el estado por si la pulsación de soltar se
			// perdió durante la carga.
			weaponState.SetState(State::kInHand);
			break;
		default:
			break;
		}
	}

	void WeaponManager::BeginAiming()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = player ? player->GetEquippedObject(false) : nullptr;
		auto* boundWeapon = weapon ? weapon->As<RE::TESBoundObject>() : nullptr;

		if (!boundWeapon) {
			return;
		}

		weaponState.SetActiveWeapon(boundWeapon);
		weaponState.SetState(State::kAiming);
	}

	void WeaponManager::ThrowWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();

		RE::ProjectileHandle handle;

		if (player && weapon) {
			// El proyectil réplica se crea antes de desequipar, mientras el
			// arma todavía está en la mano (para tomar su posición justo
			// ahí, ver Throw::LaunchWeapon), y "al mismo tiempo" (Mecanica
			// del arma.txt, punto 2) el arma original se oculta.
			handle = Throw::LaunchWeapon(player, weapon->As<RE::TESObjectWEAP>());
			weaponState.SetProjectileHandle(handle);

			// Sin cola y aplicación inmediata: un desequipar encolado podía
			// perderse en silencio si se dispara desde un evento de carga.
			RE::ActorEquipManager::GetSingleton()->UnequipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
		}

		weaponState.SetState(State::kThrown);

		if (handle) {
			// Empieza a vigilar el proyectil recién lanzado para detectar
			// impacto (punto 6) o distancia máxima sin impactar (punto 5);
			// ver Throw::TrackProjectile.
			Throw::TrackProjectile(handle);
		}
	}

	void WeaponManager::RecallWeapon()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		auto* weapon = weaponState.GetActiveWeapon();

		if (player && weapon) {
			// Se difiere al siguiente tick (tarea de SKSE) en vez de
			// llamarlo aquí mismo: invocado justo al cerrarse una pantalla
			// de carga, el juego aceptaba la orden (sonaba el sonido de
			// equipar) pero nunca llegaba a equipar el arma de verdad.
			SKSE::GetTaskInterface()->AddTask([player, weapon]() {
				RE::ActorEquipManager::GetSingleton()->EquipObject(player, weapon, nullptr, 1, nullptr, false, true, true, true);
			});
		}

		// Proceso inverso al lanzamiento (punto 2 de Mecanica del
		// arma.txt): la réplica desaparece al recuperar el arma original.
		// Sin esto, el Projectile nativo (todavía en vuelo, o clavado en
		// una superficie) se quedaba para siempre en el mundo como un arma
		// fantasma (comprobado en el juego). No cubre el caso de clavado
		// en un actor: ahí el handle guardado deja de resolver a nada, y
		// tampoco se encuentra buscando por RE::Projectile::Manager ni por
		// radio en el mundo (las dos vías probadas y descartadas, ver
		// CLAUDE.md) — el motor parece destruir la referencia al embeberse
		// en el esqueleto y dejar solo geometría 3D reenganchada
		// directamente al actor, ya no una referencia consultable. Queda
		// pendiente para RETURN, que tomará el objeto visual directamente
		// en vez de depender de Kill() sobre una referencia.
		if (auto projectile = weaponState.GetProjectileHandle().get()) {
			projectile->Kill();
		}

		weaponState.SetActiveWeapon(nullptr);
		weaponState.SetProjectileHandle({});
		weaponState.SetState(State::kInHand);
	}
}