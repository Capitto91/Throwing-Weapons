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
}