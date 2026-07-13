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
		weaponState.SetImpactTarget({});
		weaponState.SetState(State::kInHand);
	}

	void WeaponManager::OnProjectileImpact(RE::TESObjectREFR* a_target)
	{
		if (weaponState.GetState() == State::kThrown) {
			if (a_target) {
				weaponState.SetImpactTarget(RE::ObjectRefHandle(a_target));
			}
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
		// fantasma (comprobado en el juego).
		if (auto projectile = weaponState.GetProjectileHandle().get()) {
			projectile->Kill();
		} else if (auto* target = weaponState.GetImpactTarget().get().get()) {
			// Clavado en un actor: aquí el ProjectileHandle guardado nunca
			// resuelve a nada (el motor destruye la referencia al
			// procesar el golpe) y ya no hay ninguna referencia del
			// motor que buscar (comprobado: 0 resultados tanto en
			// RE::Projectile::Manager como por radio en el mundo). Lo
			// único que queda es un nodo 3D reenganchado directamente al
			// esqueleto del actor; Throw::DetachEmbeddedWeapon lo busca y
			// lo desengancha.
			Throw::DetachEmbeddedWeapon(target);
		}

		weaponState.SetActiveWeapon(nullptr);
		weaponState.SetProjectileHandle({});
		weaponState.SetImpactTarget({});
		weaponState.SetState(State::kInHand);
	}
}