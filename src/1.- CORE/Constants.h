// Contiene valores constantes utilizados globalmente por el plugin.
// Incluye límites de distancia, velocidades, tiempos máximos y parámetros
// generales de comportamiento del arma.
//
// Varios valores se reaprovechan tal cual de la iteración anterior (ver
// CHANGELOG.md), ya calibrados en el juego. Los marcados como "placeholder"
// son nuevos en esta reescritura (sustituyen a datos que antes venía leídos
// de un formulario Projectile de la Creation Kit, ya no usado, ver
// CLAUDE.md) y no están especificados por Mecanica del arma.txt: se
// ajustarán tras probar en el juego (Fase 4 en adelante).

#pragma once

#include <chrono>
#include <string_view>

namespace Constants
{
	// EditorID de la Keyword (creada en la Creation Kit) que identifica al
	// arma arrojadiza única soportada por este plugin.
	inline constexpr std::string_view kThrowableWeaponKeyword{ "WAF_ThrowableWeapon" };

	// Ruta del archivo de configuración de controles, relativa a la carpeta
	// de instalación del juego.
	inline constexpr const char* kInputConfigPath = "Data/SKSE/Plugins/ThrowingWeapons.ini";

	// Intervalo real de sondeo del bucle de movimiento manual (ida y
	// vuelta comparten la misma primitiva, ver CLAUDE.md "patrón de
	// control manual") — ~60 ticks/segundo, mismo valor usado para el
	// regreso en la iteración anterior.
	inline constexpr std::chrono::milliseconds kTickInterval{ 16 };
	inline constexpr float                     kTickDeltaSeconds = 0.016f;

	// -- Ida (THROW), punto 3 de Mecanica del arma.txt --
	// Placeholder: antes se leían de BGSProjectile::data.speed/gravity de
	// un formulario Projectile de la Creation Kit; al no depender ya de
	// ningún Projectile, la parábola es una simulación propia y necesita
	// sus propias constantes. kThrowGravity usa el valor de gravedad
	// estándar de Havok en Skyrim (documentado en la comunidad de
	// modding, no medido por nosotros); kThrowInitialSpeed es un punto de
	// partida redondo pendiente de ajustar en el juego.
	inline constexpr float kThrowInitialSpeed = 3000.0f; // u/s, placeholder
	inline constexpr float kThrowGravity = -1071.816f;   // u/s^2, gravedad estándar de Havok en Skyrim

	// Radio del barrido en cruz de la colisión en vuelo
	// (Collision::SweepRaycast): varias muestras cercanas entre sí, en vez
	// de un único rayo, detectan de forma más fiable geometría irregular o
	// un actor en movimiento, y encuentran un punto de contacto más
	// superficial (reduce cuánto se hunde el arma clavada en la malla,
	// comprobado en el juego). Placeholder aproximando el tamaño real de
	// la cabeza del arma, pendiente de ajustar en el juego.
	inline constexpr float kThrowCollisionRadius = 25.0f;

	// El punto de impacto que devuelve el raycast es donde el rayo (línea
	// infinitamente fina) cruza la superficie; si se coloca ahí el origen
	// del modelo, parte de la malla del arma (que tiene volumen hacia
	// delante) queda hundida dentro de la superficie — comprobado en el
	// juego. Se retrocede el punto de clavado esta distancia a lo largo de
	// la dirección de vuelo. Placeholder aproximando la mitad del tamaño
	// del arma, pendiente de ajustar en el juego.
	inline constexpr float kStickEmbedBackoff = 15.0f;

	// Distancia máxima de lanzamiento (punto 5): si no impacta contra
	// nada, el arma regresa automáticamente. Placeholder de partida (en la
	// iteración anterior se dobló a 12000 para compensar un hueco visual
	// específico de la migración Projectile→réplica al iniciar el
	// regreso automático; con réplica propia desde el lanzamiento ese
	// motivo ya no aplica, así que se vuelve a partir del valor original).
	inline constexpr float kMaxThrowDistance = 6000.0f;

	// -- Regreso (RETURN), puntos 7-8 --
	// Aceleración de regreso por defecto y límite de duración: el arma
	// parte con velocidad cero y acelera de forma constante; si a esta
	// aceleración tardaría más de kReturnMaxDuration en volver desde
	// donde esté, se aumenta lo necesario para cumplir el límite.
	// Reutilizados tal cual, ya calibrados en el juego.
	inline constexpr float kReturnAcceleration = 3000.0f;
	inline constexpr float kReturnMaxDuration = 2.0f;

	// Distancia a la que se considera que el arma ha llegado a la mano del
	// jugador durante el regreso. Reutilizado tal cual.
	inline constexpr float kReturnArrivalDistance = 30.0f;

	// Desviación lateral del punto de control de la curva de Bezier
	// cuadrática del regreso (punto 7: nunca en línea recta), como
	// fracción de la distancia total, acotada en unidades absolutas.
	// Reutilizados tal cual.
	inline constexpr float kReturnCurveLateralFraction = 0.28f;
	inline constexpr float kReturnCurveMinOffset = 60.0f;
	inline constexpr float kReturnCurveMaxOffset = 500.0f;

	// -- Giro y enderezado (punto 10) --
	// Velocidad de giro sobre sí misma durante el vuelo y distancia a la
	// que empieza a enderezarse en vez de seguir girando. Reutilizados
	// tal cual (calibrados en el juego contra el mismo modelo soportado).
	inline constexpr float kSpinDegreesPerSecond = 1440.0f;
	inline constexpr float kReturnStraightenDistance = 250.0f;

	// Corrección de "montaje" del modelo 3D soportado: girado 90° sobre su
	// propio eje de alabeo (Y, el eje mango→cabeza) para que se vea "de
	// lado" en vez de "estirado" hacia la dirección de vuelo durante el
	// giro. Ajuste empírico específico de este modelo — reverificar contra
	// las cajas bhkBoxShape del NIF si se cambia de arma soportada.
	inline constexpr float kModelRollOffset = 1.5707963267948966f; // 90°

	// -- Temblor al clavarse (punto 11) --
	// Duración de la vibración antes de desprenderse al iniciar el
	// regreso desde un objetivo clavado. Valor dado explícitamente por
	// Mecanica del arma.txt (no es un placeholder).
	inline constexpr float kStickShudderDuration = 0.1f;
}
