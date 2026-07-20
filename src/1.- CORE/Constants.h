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
#include <cstdint>
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

	// Mejora Kratos #1 (ver PLAN-mejoras-kratos.md): rampa lineal de
	// gravedad al salir de la mano (0 -> kThrowGravity durante este tiempo,
	// luego constante), para que la trayectoria salga más "plana" en vez de
	// empezar a caer desde el primer instante. Placeholder, sin valor de
	// referencia previo -- pendiente de ajustar en el juego. <= 0.0f
	// desactiva la rampa (comportamiento anterior, gravedad constante desde
	// el instante cero).
	inline constexpr float kThrowGravityRampDuration = 0.15f;  // s, placeholder

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

	// Mejora Kratos #4 (PLAN-mejoras-kratos.md), campo 1: fracción de
	// anclaje del punto de control a lo largo de la línea inicio→mano (no
	// confundir con kReturnCurveLateralFraction*, que controla la
	// magnitud del desplazamiento lateral desde ese punto de anclaje, ver
	// Return::ComputeReturnControlPoint) -- 1/3 en vez del 0.5 implícito
	// de antes, para que el punto de control quede cerca del origen en
	// vez de en el medio.
	inline constexpr float kReturnCurveAnchorFraction = 1.0f / 3.0f;

	// Mejora Kratos #3 (PLAN-mejoras-kratos.md): transición de captura al
	// llegar de verdad a la mano (Animation::PlayCaptureTransition,
	// WeaponManager::ReequipAfterCapture) -- desplazamiento local del clon
	// de la réplica respecto al nodo de la mano mientras dura la
	// transición. Rotación local sin parametrizar todavía (se deja en
	// identidad, la más simple posible como punto de partida) -- si hace
	// falta una rotación fija distinta, añadir aquí. Placeholders, sin
	// valor de referencia previo -- pendientes de ajustar en el juego.
	inline constexpr RE::NiPoint3               kCaptureTransitionLocalOffset{ 0.0f, 0.0f, 0.0f };
	inline constexpr std::chrono::milliseconds kCaptureTransitionDuration{ 300 };

	// Ventana real que se deja activa la variable de animation graph
	// "SkipEquipAnimation" (mod externo del mismo nombre, ver CLAUDE.md)
	// antes de desactivarla en WeaponManager::ReequipActiveWeapon --
	// activarla y desactivarla en el mismo tick que EquipObject no bastaba
	// (confirmado: SetGraphVariableBool sí tenía éxito, pero la animación
	// seguía reproduciéndose), probablemente porque EquipObject no procesa
	// el equipado de verdad de forma síncrona (ver CLAUDE.md, mismo motivo
	// por el que ya se difiere un tick). Mismo patrón hilo-que-duerme-y-
	// reencola de todo el proyecto. Placeholder, sin valor de referencia
	// previo -- pendiente de ajustar en el juego.
	inline constexpr std::chrono::milliseconds kSkipEquipAnimationWindow{ 500 };

	// -- Giro durante el vuelo (punto 10) --
	// Nombre del nodo hijo, dedicado solo al giro visual, dentro del NIF
	// del arma (ver 8.- ANIMATION/WeaponAnimation y CLAUDE.md) — no el nodo
	// raíz, para no competir con SetAngle/Update3DPosition, que el propio
	// código reescribe cada tick sobre el nodo raíz para mover la réplica.
	// Única fuente de verdad compartida con el NIF: debe coincidir exacto
	// con el nombre que se le dé al nodo en NifSkope.
	inline constexpr std::string_view kWeaponSpinNodeName{ "Mjolnir" };

	// Velocidad angular del giro (radianes/segundo) y eje local sobre el
	// que gira (ver 8.- ANIMATION/WeaponAnimation::TickSpin). Calculado y
	// escrito directamente por código cada tick (NiMatrix3::MakeRotation),
	// sin depender de ninguna animación horneada en el NIF ni de
	// NiTimeController -- confirmado en el juego que llamar a
	// NiTimeController::Update() fuera del propio recorrido del motor
	// puede crashear (ver CHANGELOG.md), así que se evita esa clase
	// entera de API. Placeholders pendientes de ajustar en el juego: la
	// velocidad es una vuelta completa cada ~0.5s (a ojo, sin medir), y el
	// eje asume que el modelo tiene el mango a lo largo del eje Y local
	// (convención habitual de armas en Skyrim) y por tanto gira mejor
	// sobre X -- si el giro se ve raro, es el primer valor a revisar.
	//
	// kSpinAxisLocal debe ser un vector unitario: NiMatrix3::MakeRotation
	// (lib/commonlibsse-ng/src/RE/N/NiMatrix3.cpp) implementa la fórmula de
	// Rodrigues, que asume el eje ya normalizado -- con un eje de longitud
	// distinta de 1 la matriz resultante deja de ser una rotación pura y
	// mete un escalado que varía con el ángulo (el arma se ve "aplastada",
	// casi en 2D, en ciertos puntos del giro; reportado por el usuario).
	// {0,0,0.7f} (longitud 0.7, bug) corregido a {0,0,1.0f} -- misma
	// dirección, magnitud unitaria.
	inline constexpr float        kSpinAngularSpeed = 20.0f;  // ~4*pi rad/s
	inline constexpr RE::NiPoint3 kSpinAxisLocal{ 0.0f, 0.0f, 1.0f };

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
	// Mejora Kratos #2 (PLAN-mejoras-kratos.md): magnitud del stagger
	// escrito directamente en el animation graph del actor golpeado
	// (Combat::ApplyReturnHit, SetGraphVariableFloat("staggerMagnitude", ...)
	// + NotifyAnimationGraph("staggerStart")), sustituyendo al hechizo
	// propio (Ability/Constant Effect) que se concedía y retiraba antes.
	// Placeholder, sin valor de referencia previo -- pendiente de ajustar
	// en el juego.
	inline constexpr float kStaggerMagnitude = 1.0f;

	// -- Temblor al clavarse (punto 11) --
	// Duración de la vibración antes de desprenderse al iniciar el
	// regreso desde un objetivo clavado. Valor dado explícitamente por
	// Mecanica del arma.txt (no es un placeholder).
	inline constexpr float kStickShudderDuration = 0.1f;

	// -- Estela visual durante el vuelo (ver PLAN-trail.md / 8.- ANIMATION/WeaponTrail) --
	// NIF del efecto de estela, con la misma convención estructural que
	// espera el código portado de Precision (nodo kTrailRootNodeName con
	// una cadena de huesos segmento ya modelados). Misma ruta que usa
	// Precision internamente (Settings::attackTrailMeshPath) -- confirmado
	// con el usuario, que ha copiado ese NIF/textura tal cual como
	// placeholder para probar la integración.
	inline constexpr const char* kTrailEffectPath = "Effects/WeaponTrails/AttackTrail.nif";

	// Nombre del nodo hijo, dentro del NIF de estela, que contiene la
	// cadena de huesos segmento a reposicionar cada tick. Única fuente de
	// verdad compartida con el NIF, igual que kWeaponSpinNodeName.
	inline constexpr std::string_view kTrailRootNodeName{ "TrailRoot" };

	// Tiempo de vida de cada segmento individual de la estela antes de
	// reciclarse (determina cuánto "rastro" se ve detrás del arma en cada
	// instante). Mismo valor de partida que Precision
	// (Settings::fTrailSegmentLifetime), pendiente de ajustar en el juego.
	inline constexpr float kTrailSegmentLifetime = 0.1f;

	// Cuántos segmentos nuevos de estela se añaden por segundo de vuelo.
	// Mismo valor de partida que Precision
	// (Settings::uTrailSegmentsPerSecond), pendiente de ajustar en el
	// juego.
	inline constexpr std::uint32_t kTrailSegmentsPerSecond = 120;

	// Color base de la estela (RGBA, 0-1) -- azul eléctrico a petición del
	// usuario, sustituye al gris neutro que trae Precision por defecto
	// (Settings::fTrailDefaultBaseColor*).
	inline constexpr RE::NiColorA kTrailBaseColor{ 0.1f, 0.45f, 1.0f, 1.0f };

	// Multiplicador de brillo aplicado sobre kTrailBaseColor por el
	// material del shader de efecto (BSEffectShaderMaterial::baseColorScale) --
	// por encima de 1 satura hacia blanco/da aspecto de brillo (bloom),
	// buscado aquí para el efecto "eléctrico". Mismo mecanismo que
	// Precision (Settings::fTrailBaseColorScaleMult, que por defecto deja
	// en 1 sin ningún brillo extra). Placeholder, pendiente de ajustar en
	// el juego.
	inline constexpr float kTrailBaseColorScaleMult = 3.0f;

	// Escala aplicada a cada segmento de la estela. El código nunca la
	// calculaba a partir de la malla del arma (a diferencia de Precision,
	// que la deriva del alcance del arma equipada -- descartado al portar,
	// ver 8.- ANIMATION/WeaponTrail.cpp): sin esto, cada segmento se
	// quedaba con la escala que trae por defecto el NIF de Precision, sin
	// ninguna relación con el tamaño real de la malla propia. 1.0 (probado
	// en el juego) resultó desproporcionado -- Precision nunca usa la
	// malla a esa escala tal cual, siempre la reduce en proporción al
	// alcance del arma equipada (longitud * 0.01, típicamente muy por
	// debajo de 1). Placeholder bajado en consecuencia, sigue pendiente de
	// ajuste fino en el juego.
	inline constexpr float kTrailSegmentScale = 0.15f;

	// Punto desde el que "nace" visualmente la estela, expresado en el
	// espacio local del arma (WeaponTrail lo transforma por la rotación
	// de la réplica en cada muestra, para que la acompañe si cambia de
	// orientación) en vez de usar directamente el origen del nodo raíz
	// (node3D->world) -- en armas de Skyrim el origen suele coincidir con
	// el punto de agarre/mango, no con el centro visual de la pieza.
	// Comprobado en NifSkope en el NIF de este arma (un martillo): el
	// origen cae en la base del mango. Ajustado a ojo en el juego por el
	// usuario (0 en X/Z, +20 en Y).
	inline constexpr RE::NiPoint3 kTrailAnchorLocalOffset{ 0.0f, 20.0f, 0.0f };
}
