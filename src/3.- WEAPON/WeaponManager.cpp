// Implementación del ciclo de vida del arma.
// Coordina la transición entre arma en mano, arma lanzada y arma recuperada.

#include "3.- WEAPON/WeaponManager.h"

#include "1.- CORE/Constants.h"
#include "4.- THROW/ThrowManager.h"
#include "4.- THROW/ThrowProjectile.h"
#include "5.- RETURN/ReturnManager.h"

#include <chrono>
#include <thread>
#include <tuple>

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
			BeginReturn();
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
		weaponState.SetReturnReplicaHandle({});
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

	void WeaponManager::OnReturnComplete()
	{
		ReequipAndReset();
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
		// Proceso inverso al lanzamiento (punto 2 de Mecanica del
		// arma.txt): la réplica desaparece al recuperar el arma original.
		// Sin esto, el Projectile nativo (todavía en vuelo, o clavado en
		// una superficie) se quedaba para siempre en el mundo como un arma
		// fantasma (comprobado en el juego).
		if (auto projectile = weaponState.GetProjectileHandle().get()) {
			projectile->Kill();
		} else if (auto replica = weaponState.GetReturnReplicaHandle().get()) {
			// Regreso ya en marcha (ver BeginReturn/SpawnReplicaAndBeginReturn):
			// la réplica visual no es un Projectile, se borra como
			// cualquier referencia normal.
			replica->Disable();
			replica->SetDelete(true);
		} else if (auto* target = weaponState.GetImpactTarget().get().get()) {
			// Clavado en un actor: aquí el ProjectileHandle guardado nunca
			// resuelve a nada (el motor destruye la referencia al
			// procesar el golpe) y ya no hay ninguna referencia del
			// motor que buscar (comprobado: 0 resultados tanto en
			// RE::Projectile::Manager como por radio en el mundo). Lo
			// único que queda es un nodo 3D reenganchado directamente al
			// esqueleto del actor; Throw::DetachEmbeddedWeapon lo busca y
			// lo desengancha (no necesitamos la posición devuelta aquí).
			std::ignore = Throw::DetachEmbeddedWeapon(target);
		}

		ReequipAndReset();
	}

	void WeaponManager::BeginReturn()
	{
		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			// Sin jugador no hay a quién volver; cae al placeholder
			// instantáneo como red de seguridad.
			RecallWeapon();
			return;
		}

		// A partir de aquí ya no se vuelve a entrar por este botón mientras
		// se resuelve el punto de partida (OnAimButtonDown solo llama a
		// BeginReturn() en kThrown/kStuck): evita arrancar dos intentos de
		// regreso en paralelo si se pulsa varias veces seguidas mientras
		// TryDetachAndBeginReturn todavía está reintentando.
		weaponState.SetState(State::kReturning);

		if (auto projectile = weaponState.GetProjectileHandle().get()) {
			// En vuelo o clavado en superficie: se captura la posición
			// actual, se destruye el Projectile nativo y se lanza la
			// réplica visual que Return va a controlar (ver comentario en
			// el header: un Projectile todavía activo en Havok compite
			// con el control manual tick a tick).
			const auto position = projectile->GetPosition();
			projectile->Kill();
			SpawnReplicaAndBeginReturn(player, position);
			return;
		}

		// Clavado en un actor: no hay Projectile vivo del que partir (ver
		// "Arquitectura de física de proyectiles" en CLAUDE.md).
		// TryDetachAndBeginReturn desengancha el modelo clavado (con
		// reintentos) y lanza la réplica en esa posición.
		auto targetHandle = weaponState.GetImpactTarget();
		if (targetHandle.get().get()) {
			TryDetachAndBeginReturn(targetHandle, Constants::kEmbeddedWeaponDetachMaxAttempts);
		} else {
			RecallWeapon();
		}
	}

	void WeaponManager::TryDetachAndBeginReturn(RE::ObjectRefHandle a_targetHandle, int a_attemptsLeft)
	{
		if (weaponState.GetState() != State::kReturning) {
			// El ciclo cambió por otra vía mientras se reintentaba (p. ej.
			// una pantalla de carga resincronizó el estado); no hay nada
			// que retomar aquí.
			return;
		}

		auto* target = a_targetHandle.get().get();
		auto  position = target ? Throw::DetachEmbeddedWeapon(target) : std::nullopt;

		if (!position) {
			if (a_attemptsLeft > 0) {
				// El nodo "Scene Root" puede tardar hasta ~1s en aparecer
				// tras el impacto (ver Constants::kEmbeddedWeaponNodeName);
				// se reintenta en segundo plano en vez de rendirse a la
				// primera (comprobado en el juego: sin esto, el arma se
				// queda enganchada para siempre si se pulsa recuperar antes
				// de ese margen).
				std::thread([this, a_targetHandle, a_attemptsLeft]() {
					std::this_thread::sleep_for(Constants::kEmbeddedWeaponDetachRetryInterval);
					SKSE::GetTaskInterface()->AddTask([this, a_targetHandle, a_attemptsLeft]() {
						TryDetachAndBeginReturn(a_targetHandle, a_attemptsLeft - 1);
					});
				}).detach();
			} else {
				RecallWeapon();
			}
			return;
		}

		auto* player = RE::PlayerCharacter::GetSingleton();
		if (!player) {
			RecallWeapon();
			return;
		}

		// El nodo desenganchado está literalmente encima/dentro del cuerpo
		// del actor (ahí es donde estaba clavado); lanzar la réplica justo
		// ahí puede solapar su colisión con la del actor y hacer que el
		// motor la vuelva a clavar de forma nativa antes de que
		// Return::BeginReturn llegue a tomar control (hipótesis a probar:
		// no confirmado en el juego todavía). Se separa un poco hacia
		// arriba para evitar ese solape.
		SpawnReplicaAndBeginReturn(player, *position + RE::NiPoint3{ 0.0f, 0.0f, 40.0f });
	}

	void WeaponManager::SpawnReplicaAndBeginReturn(RE::Actor* a_player, const RE::NiPoint3& a_position)
	{
		auto* weapon = weaponState.GetActiveWeapon();
		auto  handle = weapon ? Throw::SpawnWeaponReplicaAt(a_player, weapon->As<RE::TESObjectWEAP>(), a_position) : RE::ObjectRefHandle{};

		logs::info("SpawnReplicaAndBeginReturn: réplica en ({:.1f},{:.1f},{:.1f}) -> handle {}", a_position.x, a_position.y, a_position.z, handle.get() != nullptr);

		if (!handle.get()) {
			RecallWeapon();
			return;
		}

		weaponState.SetReturnReplicaHandle(handle);
		Return::BeginReturn(a_player, handle);
	}

	void WeaponManager::ReequipAndReset()
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

		weaponState.SetActiveWeapon(nullptr);
		weaponState.SetProjectileHandle({});
		weaponState.SetImpactTarget({});
		weaponState.SetReturnReplicaHandle({});
		weaponState.SetState(State::kInHand);
	}
}