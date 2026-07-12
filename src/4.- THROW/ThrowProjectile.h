// Vigila el proyectil réplica mientras está en vuelo, para detectar los
// eventos que 3.- WEAPON necesita: impacto (transición a "clavada") y
// distancia máxima superada sin impactar (regreso automático).

#pragma once

namespace Throw
{
	// Empieza a vigilar, sondeando cada pocos milisegundos, el proyectil
	// identificado por a_handle:
	// - Si impacta contra algo (enemigo o superficie, punto 6 de Mecanica
	//   del arma.txt), notifica a Weapon::WeaponManager::OnProjectileImpact
	//   para pasar a "clavada".
	// - Si supera Constants::kMaxThrowDistance sin impactar (punto 5),
	//   notifica a Weapon::WeaponManager::OnProjectileMaxRangeReached para
	//   iniciar la recuperación automática.
	// Deja de vigilar en cuanto el arma sale del estado "lanzada" por
	// cualquier otro motivo (el jugador pulsa recuperar, una pantalla de
	// carga resincroniza el estado...), o si el proyectil deja de existir
	// sin que detectásemos ninguno de los dos casos anteriores (p. ej. la
	// celda se descarga). Debe llamarse justo después de lanzar el
	// proyectil (ver Throw::LaunchWeapon).
	void TrackProjectile(RE::ProjectileHandle a_handle);
}
