// Implementación de la vigilancia del proyectil réplica.
// Sondea el proyectil una vez por fotograma y notifica a Weapon::WeaponManager
// cuando detecta impacto o distancia máxima superada.

#include "4.- THROW/ThrowProjectile.h"

#include "1.- CORE/Constants.h"
#include "1.- CORE/PerfTimer.h"
#include "3.- WEAPON/WeaponManager.h"

#include <chrono>
#include <thread>

namespace Throw
{
	namespace
	{
		// No hay evento nativo de "el proyectil se ha clavado": se sondea en
		// vez de escuchar. No reprograma la siguiente comprobación llamando
		// a AddTask directamente desde dentro de sí misma: si la cola de
		// tareas de SKSE no está separada por fotogramas (o la procesa de
		// forma síncrona), eso encadena una comprobación tras otra sin ceder
		// nunca el control al motor, congelando el juego (comprobado). En su
		// lugar, cuando toca seguir vigilando, se lanza un hilo aparte que
		// duerme un intervalo de tiempo real y solo entonces reprograma la
		// comprobación en el hilo principal — así hay una separación real
		// garantizada entre una comprobación y la siguiente.
		constexpr auto kPollInterval = std::chrono::milliseconds(50);

		void PollProjectile(RE::ProjectileHandle a_handle)
		{
			auto* weaponManager = Weapon::WeaponManager::GetSingleton();
			if (weaponManager->GetState() != Weapon::State::kThrown) {
				// El ciclo salió de "lanzada" por otro motivo (el jugador
				// pulsó recuperar, resincronización tras pantalla de
				// carga...); no hay nada más que vigilar.
				return;
			}

			auto projectile = a_handle.get();
			if (!projectile) {
				// El proyectil dejó de existir (p. ej. celda descargada)
				// sin que detectásemos impacto ni distancia máxima.
				return;
			}

			// Mismo mecanismo que usan las flechas al clavarse (ver
			// CLAUDE.md, "Arquitectura de física de proyectiles"). Solo
			// kImpale (clavada en un actor) y kStick (clavada en una
			// superficie) cuentan como "clavada" (punto 6 de Mecanica del
			// arma.txt); kBounce significa que ha rebotado y sigue en
			// vuelo, no hay que darlo por terminado (comprobado: un rebote
			// contra una pared se detectaba antes como clavada por error).
			const auto* missile = skyrim_cast<RE::MissileProjectile*>(projectile.get());
			const auto  impactResult = missile ? missile->GetMissileRuntimeData().impactResult : RE::ImpactResult::kNone;

			if (impactResult == RE::ImpactResult::kImpale || impactResult == RE::ImpactResult::kStick) {
				logs::info("Proyectil: impacto detectado (ImpactResult={})", static_cast<int>(impactResult));
				weaponManager->OnProjectileImpact();
				return;
			}

			// El agua no es una superficie donde clavarse: ImpactResult no
			// cambia al caer en ella (comprobado en el juego, se queda
			// flotando/hundiéndose sin "impactar" nunca), así que hay que
			// detectarlo aparte. IsInWater() es una comprobación barata
			// (compara la altura del agua de la celda con la posición Z
			// del proyectil), apta para el sondeo periódico.
			if (projectile->IsInWater()) {
				logs::info("Proyectil: ha caído al agua, recuperando automáticamente");
				weaponManager->OnProjectileEnteredWater();
				return;
			}

			const auto distanceMoved = projectile->GetProjectileRuntimeData().distanceMoved;
			if (distanceMoved >= Constants::kMaxThrowDistance) {
				logs::info(
					"Proyectil: distancia máxima superada sin impactar ({} >= {}), recuperando automáticamente",
					distanceMoved,
					Constants::kMaxThrowDistance);
				weaponManager->OnProjectileMaxRangeReached();
				return;
			}

			{
				// Diagnóstico temporal (ver PerfTimer.h): esta creación de
				// hilo ocurre en el hilo principal, hasta 20 veces/segundo
				// mientras el arma está en vuelo.
				Perf::ScopedTimer timer{ "Throw::PollProjectile creación de hilo", std::chrono::microseconds{ 1000 } };
				std::thread([a_handle]() {
					std::this_thread::sleep_for(kPollInterval);
					SKSE::GetTaskInterface()->AddTask([a_handle]() { PollProjectile(a_handle); });
				}).detach();
			}
		}
	}

	void TrackProjectile(RE::ProjectileHandle a_handle)
	{
		SKSE::GetTaskInterface()->AddTask([a_handle]() { PollProjectile(a_handle); });
	}
}
