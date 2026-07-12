// Contiene valores constantes utilizados globalmente por el plugin.
// Incluye límites de distancia, velocidades, tiempos máximos y parámetros
// generales de comportamiento del arma.

#pragma once

#include <string_view>

namespace Constants
{
	// EditorID de la Keyword (creada en la Creation Kit) que identifica al
	// arma arrojadiza única soportada por este plugin. Se le asigna esa
	// Keyword al arma en cuestión; cualquier otra arma la ignora el sistema.
	inline constexpr std::string_view kThrowableWeaponKeyword{ "WAF_ThrowableWeapon" };

	// Ruta del archivo de configuración de controles, relativa a la carpeta
	// de instalación del juego.
	inline constexpr const char* kInputConfigPath = "Data/SKSE/Plugins/ThrowingWeapons.ini";

	// EditorID del Projectile (creado en la Creation Kit) que representa al
	// arma volando durante el lanzamiento. Debe existir como formulario
	// Projectile independiente del arma; no hay forma de generarlo en
	// tiempo de ejecución.
	inline constexpr std::string_view kThrowableProjectile{ "CAP_ThorMjolnir_Projectile" };

	// EditorID del Ammo (creado en la Creation Kit) que referencia al
	// Projectile de arriba. Se lanza vía Projectile::LaunchArrow (la misma
	// ruta que usan las flechas reales) en vez de construir el LaunchData a
	// mano: con el constructor genérico el proyectil no colisionaba con
	// nada (ni paredes), aunque la capa de colisión y el modelo eran
	// correctos — LaunchArrow configura algo más que hace falta para que
	// el motor registre los impactos.
	inline constexpr std::string_view kThrowableAmmo{ "CAP_ThorMjolnir_Ammo" };

	// Distancia máxima de lanzamiento, en unidades de juego, antes de que
	// el arma regrese automáticamente si no ha impactado contra nada
	// (Mecanica del arma.txt, punto 5). El documento no especifica un
	// valor concreto; es una decisión de diseño no cubierta por él.
	inline constexpr float kMaxThrowDistance = 6000.0f;
}