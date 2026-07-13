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

	// Nombre del nodo 3D raíz del modelo del arma tal como quedó en el NIF
	// exportado (nombre por defecto del exportador, nunca renombrado en la
	// Creation Kit). Al clavarse en un actor, el motor destruye la
	// referencia Projectile original (comprobado en el juego: el handle ya
	// no resuelve a nada, ni siquiera en el instante del impacto) y, con
	// aprox. 1 segundo de retraso, engancha este nodo directamente al
	// hueso donde impactó — ya no es una referencia del juego, solo
	// geometría. Es la única forma encontrada de localizarlo para poder
	// desengancharlo (ver Throw::DetachEmbeddedWeapon). Si se reexporta el
	// modelo con el nodo raíz renombrado, este valor deja de coincidir.
	inline constexpr std::string_view kEmbeddedWeaponNodeName{ "Scene Root" };

	// Velocidad de regreso en línea recta, en unidades de juego por
	// segundo. Fase 1 de RETURN (sin curvatura ni velocidad híbrida
	// todavía): valor plano provisional, pendiente de sustituir por el
	// cálculo del punto 8 de Mecanica del arma.txt (constante salvo que
	// tarde más de 2 segundos, en cuyo caso se acelera para cumplir ese
	// límite).
	inline constexpr float kReturnSpeed = 3000.0f;

	// Distancia, en unidades de juego, a la que se considera que el arma
	// ha llegado a la mano del jugador durante el regreso y se reequipa.
	// Sin especificar en el documento; valor pequeño elegido sin más
	// criterio que evitar que el punto de destino en movimiento (la mano
	// del jugador) provoque oscilaciones al intentar alcanzarlo con
	// precisión exacta.
	inline constexpr float kReturnArrivalDistance = 30.0f;
}