// Contiene valores constantes utilizados globalmente por el plugin.
// Incluye límites de distancia, velocidades, tiempos máximos y parámetros
// generales de comportamiento del arma.

#pragma once

#include <chrono>
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
	// Duplicada (6000 → 12000) tras comprobar en el juego que, al activarse
	// este auto-regreso, la réplica que controla 5.- RETURN tarda
	// perceptiblemente en cargar su 3D (misma espera que en cualquier otro
	// inicio de regreso, ver ReturnManager::WaitFor3DThenStart) — el hueco
	// se nota más aquí porque el arma está muy lejos del jugador, donde el
	// streaming de objetos es más lento; a esta distancia el caso ya casi
	// no se da en juego normal (algo suele impactar antes).
	inline constexpr float kMaxThrowDistance = 12000.0f;

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

	// Aceleración de regreso por defecto, en unidades de juego por segundo
	// al cuadrado (punto 8 de Mecanica del arma.txt): el arma parte con
	// velocidad cero y acelera de forma constante, en vez de moverse a
	// velocidad plana. A esta aceleración, un regreso desde
	// kMaxThrowDistance tardaría más de kReturnMaxDuration en llegar —
	// Return::ComputeReturnAcceleration se encarga de aumentarla lo
	// necesario para no superar ese límite (punto 8, aceleración
	// híbrida); para distancias menores (la mayoría de los lanzamientos)
	// este valor por defecto ya cumple el límite sin necesidad de
	// aumentarlo.
	inline constexpr float kReturnAcceleration = 3000.0f;

	// Límite de duración del regreso, en segundos (punto 8 de Mecanica del
	// arma.txt): si a kReturnAcceleration el arma tardaría más en volver,
	// se aumenta la aceleración lo necesario para cumplir este límite.
	inline constexpr float kReturnMaxDuration = 2.0f;

	// Distancia, en unidades de juego, a la que se considera que el arma
	// ha llegado a la mano del jugador durante el regreso y se reequipa.
	// Sin especificar en el documento; valor pequeño elegido sin más
	// criterio que evitar que el punto de destino en movimiento (la mano
	// del jugador) provoque oscilaciones al intentar alcanzarlo con
	// precisión exacta.
	inline constexpr float kReturnArrivalDistance = 30.0f;

	// El nodo "Scene Root" (ver kEmbeddedWeaponNodeName) tarda en aparecer
	// tras el impacto contra un actor — se ha visto tardar bastante más de
	// 1s en algunos casos en el juego. Si se pulsa recuperar antes de eso,
	// Throw::DetachEmbeddedWeapon no lo encuentra todavía;
	// Weapon::WeaponManager reintenta en vez de rendirse a la primera. Si
	// se agotan los reintentos sin encontrarlo, WeaponManager::TryDetachAndBeginReturn
	// arranca igualmente el regreso animado desde la posición del actor
	// (nunca un recall instantáneo visible para el jugador), así que
	// kEmbeddedWeaponDetachMaxAttempts solo decide cuánto se prioriza la
	// posición exacta del clavado frente a caer a esa aproximación.
	inline constexpr int                       kEmbeddedWeaponDetachMaxAttempts = 20;
	inline constexpr std::chrono::milliseconds kEmbeddedWeaponDetachRetryInterval{ 150 };

	// Intentos extra, tras agotar los de arriba, que
	// WeaponManager::WatchForLateEmbeddedWeapon sigue vigilando en segundo
	// plano si el nodo "Scene Root" no apareció a tiempo: el regreso ya ha
	// arrancado con la posición aproximada del actor a esas alturas, así
	// que esto no afecta a la trayectoria — solo evita dejar un arma
	// fantasma clavada para siempre si el nodo aparece más tarde, y deja
	// constancia en el log de cuánto tardó de verdad. Mismo intervalo que
	// kEmbeddedWeaponDetachRetryInterval; valor elegido para dar una
	// ventana de diagnóstico generosa (~6s extra) sin vigilar para
	// siempre.
	inline constexpr int kLateEmbeddedWeaponWatchAttempts = 40;

	// Desviación lateral del punto de control de la curva de Bezier
	// cuadrática del regreso, como fracción de la distancia total a
	// recorrer (punto 7 de Mecanica del arma.txt: el regreso nunca es una
	// línea recta) — ver Return::ComputeReturnControlPoint.
	// kReturnCurveMinOffset/MaxOffset acotan esa fracción en unidades
	// absolutas, para que un lanzamiento muy corto no dé una curva
	// ridículamente cerrada, ni uno muy largo una exageradamente ancha.
	// Ninguno de los tres valores está especificado por el documento.
	inline constexpr float kReturnCurveLateralFraction = 0.28f;
	inline constexpr float kReturnCurveMinOffset = 60.0f;
	inline constexpr float kReturnCurveMaxOffset = 500.0f;
}