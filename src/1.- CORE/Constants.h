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
	inline constexpr float kThrowInitialSpeed = 3000.0f;  // u/s, placeholder
	inline constexpr float kThrowGravity = -1071.816f;    // u/s^2, gravedad estándar de Havok en Skyrim

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

	// Igual que kStickEmbedBackoff pero al revés: contra un actor, la
	// capa golpeada (CharController) es una cápsula de colisión más
	// grande que la malla visual real, muy notable en objetivos pequeños
	// (lobos, etc.) — retroceder como con una pared deja el arma flotando
	// todavía más lejos del cuerpo (comprobado en el juego). En vez de
	// retroceder, se avanza esta distancia a lo largo de la dirección de
	// vuelo para compensar. Placeholder, pendiente de ajustar en el juego.
	inline constexpr float kActorStickForwardOffset = 15.0f;

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
	// Horquilla en vez de un valor único (ajuste pedido tras probar en
	// el juego: la curva se veía demasiado amplia y siempre igual) —
	// Return::ComputeReturnControlPoint sortea un valor uniforme dentro
	// de este rango en cada regreso, para que varíe de una vez a otra en
	// vez de ser siempre la misma curva. Rango y topes absolutos
	// reducidos respecto al valor fijo anterior (0.28 / 60-500) para
	// cerrar el ángulo general.
	inline constexpr float kReturnCurveLateralFractionMin = 0.10f;
	inline constexpr float kReturnCurveLateralFractionMax = 0.18f;
	inline constexpr float kReturnCurveMinOffset = 40.0f;
	inline constexpr float kReturnCurveMaxOffset = 260.0f;

	// -- Giro durante el vuelo (punto 10) --
	// Nombre del nodo hijo, dedicado solo al giro visual, dentro del NIF
	// del arma (ver 8.- ANIMATION/WeaponAnimation y CLAUDE.md) — no el nodo
	// raíz, para no competir con SetAngle/Update3DPosition, que el propio
	// código reescribe cada tick sobre el nodo raíz para mover la réplica.
	// Única fuente de verdad compartida con el NIF: debe coincidir exacto
	// con el nombre que se le dé al nodo en NifSkope.
	inline constexpr std::string_view kWeaponSpinNodeName{ "Mjolnir" };

	// Margen de reintentos para encontrar el nodo de giro justo tras crear
	// la réplica (Animation::StartSpinWhenReady): Physics::SpawnReplica ya
	// espera a que Get3D() deje de ser nulo, pero eso no garantiza que todo
	// el subárbol -- incluido el nodo de giro -- haya terminado de cargar
	// en ese mismo instante. Comprobado en el juego: en muy pocos
	// lanzamientos el arma salía sin girar por esto. Mismo intervalo de
	// sondeo que kTickInterval.
	inline constexpr int kWeaponSpinNodeWaitAttempts = 10;

	// -- Impacto en actor (punto 6) --
	// EditorID del hechizo de parálisis propio (creado en la Creation
	// Kit, copia del efecto vanilla de Parálisis) que se concede al actor
	// golpeado mientras el arma siga clavada. Debe crearse como tipo
	// Ability, con el efecto en modo "Constant Effect" y alcance "Self" —
	// así, concedido con Actor::AddSpell, se aplica de inmediato y de
	// forma continua sin necesidad de volver a lanzarlo, y se quita al
	// instante con Actor::RemoveSpell al recuperar el arma (ver
	// Combat::EndEmbeddedEffect). Se prefiere a lanzar el hechizo vanilla
	// real vía MagicCaster::CastSpellImmediate (primer intento,
	// descartado): esa llamada es virtual, y AddSpell/RemoveSpell no lo
	// son, además de no depender de refrescar una duración limitada.
	inline constexpr std::string_view kEmbeddedParalysisSpell{ "CAP_ThorMjolnir_Ability_ThrowingParalysis" };

	// EditorID del propio efecto mágico (EffectSetting) dentro del
	// hechizo de arriba — no el hechizo en sí. Se usa para comprobar con
	// MagicTarget::HasMagicEffect si el efecto ha quedado realmente activo
	// en el objetivo tras concederle la habilidad: AddSpell siempre tiene
	// éxito aunque la condición del propio efecto (inmune a parálisis,
	// dragón...) impida que se aplique de verdad, así que hay que
	// comprobarlo aparte en vez de asumir que funcionó.
	inline constexpr std::string_view kEmbeddedParalysisEffect{ "CAP_ThorMjolnir_ParalysisAbilityEffect" };

	// Intervalo del daño eléctrico continuo (punto 6) mientras el arma
	// siga clavada en un actor. Placeholder, pendiente de ajustar en el
	// juego.
	inline constexpr float kEmbeddedDamageInterval = 1.5f;

	// Duración máxima clavada en un actor (nerfeo pedido tras las primeras
	// pruebas: sin esto, dejar el arma clavada mucho tiempo era demasiado
	// fuerte). Pasado este tiempo, el arma regresa automáticamente aunque
	// no se pulse el botón.
	inline constexpr float kEmbeddedMaxDuration = 5.0f;

	// Margen máximo tras conceder la habilidad para confirmar que el
	// efecto ha quedado activo de verdad (MagicTarget::HasMagicEffect,
	// comprobado cada tick hasta confirmarse o agotar este margen): el
	// motor necesita al menos un tick para procesar la habilidad recién
	// concedida. Si no se confirma dentro de este margen, se interpreta
	// como objetivo inmune y el arma regresa automáticamente sin esperar
	// los kEmbeddedMaxDuration completos. Placeholder generoso, pendiente
	// de ajustar en el juego (al ser una comprobación exacta y no una
	// inferencia, se puede acortar con seguridad si se confirma que el
	// efecto tarda menos en reflejarse).
	inline constexpr float kImmunityCheckDelay = 0.3f;

	// -- Golpear durante el regreso (punto 9) --
	// Hechizo propio (Ability, Constant Effect, Self) cuyo unico efecto
	// (EffectSetting Archetype=Stagger, CAP_ThorMjolnir_MagicEffect_Stagger)
	// empuja/aturde al actor en el instante en que se concede
	// (ActiveEffect::Start de RE::StaggerEffect, confirmado que existe
	// como clase en commonlibsse-ng) -- no hace falta ninguna logica
	// continua, a diferencia de la paralisis. Se concede con
	// Actor::AddSpell (no virtual, mismo motivo que
	// kEmbeddedParalysisSpell) y se retira poco despues
	// (kStaggerSpellDuration) para no dejarlo colgado en la lista de
	// hechizos del actor -- el empujon ya ha ocurrido mucho antes de que
	// expire este margen, no hace falta esperar a que el jugador
	// recupere el arma como con la paralisis.
	inline constexpr std::string_view          kStaggerSpell{ "CAP_ThorMjolnir_Ability_ThrowingStagger" };
	inline constexpr std::chrono::milliseconds kStaggerSpellDuration{ 200 };

	// -- Temblor al clavarse (punto 11) --
	// Duración de la vibración antes de desprenderse al iniciar el
	// regreso desde un objetivo clavado. Valor dado explícitamente por
	// Mecanica del arma.txt (no es un placeholder).
	inline constexpr float kStickShudderDuration = 0.1f;
}
