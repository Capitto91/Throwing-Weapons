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
	// Cambio de criterio sobre el punto 8 original de Mecanica del
	// arma.txt (ya actualizado ahí): decisión tomada con el usuario de que
	// el regreso ya no acelera de forma constante, sino con una
	// aceleración *creciente* (perfil d(t) = kReturnAcceleration /
	// (n·(n-1)) · t^n, ver kReturnAccelerationExponent y
	// Return::ComputeTraveledDistance) -- simula un tirón magnético cada
	// vez más fuerte según se acerca a la mano, en vez de un tirón parejo
	// durante todo el trayecto. Con kReturnAccelerationExponent = 2 esta
	// fórmula colapsa exactamente en la aceleración constante anterior
	// (d = ½·a·t²), así que kReturnAcceleration conserva el mismo
	// significado físico de siempre (el valor de aceleración que se
	// alcanzaría tras 1s de rampa), solo cambia cómo se llega a él.
	// kReturnMaxDuration sigue límitando la duración total (contada desde
	// que se desprende del todo, sin el temblor del punto 11) exactamente
	// igual que antes -- ver Return::ComputeReturnAcceleration para el
	// recálculo híbrido si a este ritmo tardaría más. Subido de 3000 a
	// 4500 a petición del usuario: con el exponente creciente
	// (kReturnAccelerationExponent > 2) el mismo coeficiente de antes
	// tarda más en alcanzar velocidad de crucero que con aceleración
	// constante, y el regreso se sentía demasiado lento -- este ajuste
	// compensa esa diferencia sin tocar la forma de la curva (el "más
	// imán cuanto más cerca" se mantiene, solo más rápido en conjunto).
	//
	// ATENCIÓN al tocar esta constante (o kReturnAccelerationExponent/
	// kReturnMaxDuration más abajo): Return::ComputeReturnDuration predice
	// la duración total del regreso a partir de estos mismos valores, y esa
	// predicción es lo que fija el instante en que se dispara el sonido de
	// atrape con antelación (Constants::kCatchImpactSoundLeadTime,
	// Return::BeginReturnMovement) para que el golpe grabado en el audio
	// coincida con la llegada real. Cambiar la velocidad del regreso sin
	// volver a probar ese sonido en el juego puede desincronizarlo.
	inline constexpr float kReturnAcceleration = 4500.0f;
	inline constexpr float kReturnMaxDuration = 2.0f;

	// Exponente del perfil de aceleración creciente de arriba. 2 recupera
	// la aceleración constante de siempre; valores mayores hacen que el
	// arma empiece más despacio y tire cada vez más fuerte cuanto más
	// cerca está de la mano (más "imán", menos "empujón parejo") -- 3
	// sería aceleración que crece en línea recta con el tiempo ("jerk"
	// constante). Placeholder intermedio entre ambos, pendiente de
	// calibrar en el juego.
	inline constexpr float kReturnAccelerationExponent = 2.5f;

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

	// Ventana real que se deja activa la variable de animation graph
	// "SkipEquipAnimation" (mod externo del mismo nombre, ver CLAUDE.md)
	// antes de desactivarla en WeaponManager::ReequipAndReset --
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

	// Velocidad angular máxima del giro (radianes/segundo) y eje local
	// sobre el que gira (ver 8.- ANIMATION/WeaponAnimation::TickSpin).
	// Calculado y escrito directamente por código cada tick
	// (NiMatrix3::MakeRotation), sin depender de ninguna animación
	// horneada en el NIF ni de NiTimeController -- confirmado en el juego
	// que llamar a NiTimeController::Update() fuera del propio recorrido
	// del motor puede crashear (ver CHANGELOG.md), así que se evita esa
	// clase entera de API. Placeholders pendientes de ajustar en el juego:
	// la velocidad es una vuelta completa cada ~0.5s (a ojo, sin medir), y
	// el eje asume que el modelo tiene el mango a lo largo del eje Y local
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

	// Duración de la rampa de arranque del giro -- a petición del usuario,
	// la velocidad angular deja de ser constante desde el primer instante:
	// sube en línea recta desde 0 hasta kSpinAngularSpeed a lo largo de
	// esta duración (aceleración angular constante durante la rampa,
	// después velocidad angular constante) -- mismo patrón de dos tramos
	// en forma cerrada que Throw::ComputeGravityDrop, un orden de derivada
	// más abajo (ahí se rampeaba la aceleración lineal, aquí se rampea la
	// velocidad angular). Se aplica igual en la ida y en la vuelta
	// (Animation::TickSpin es compartido, ver ThrowManager/ReturnManager),
	// cada una con su propio "elapsed" desde cero -- el giro arranca
	// gradual en cada tramo de vuelo por separado. <= 0.0f desactiva la
	// rampa (velocidad angular constante desde el instante cero, como
	// antes). Placeholder, pendiente de ajustar en el juego.
	inline constexpr float kSpinRampDuration = 0.3f;  // s, placeholder

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
	// regreso desde un objetivo clavado. Mecanica del arma.txt da 0,1s
	// explícitamente, pero a ese valor el usuario no apreciaba el efecto en
	// el juego (probablemente por el bug de Update3DPosition corregido en
	// Animation::TickShudder/Return::BeginReturn -- ver CHANGELOG.md, no
	// por la duración en sí) -- subido a 0,5s a petición expresa para
	// confirmar visualmente que el temblor ocurre antes de recortarlo de
	// vuelta hacia el valor del documento.
	inline constexpr float kStickShudderDuration = 0.5f;

	// Ángulo máximo (radianes) que alcanza la amplitud de la oscilación --
	// no es un ángulo fijo: la amplitud crece exponencialmente desde cero
	// hasta este máximo a lo largo del temblor (ver
	// kStickShudderAmplitudeRampFraction/Animation::TickShudder), a
	// petición del usuario tras confirmar en el juego que una amplitud
	// constante se notaba demasiado poco. Simulando el tirón magnético que
	// la va aflojando de la superficie antes de soltarse del todo. Escrito
	// sobre el mismo nodo de giro visual que Animation::TickSpin
	// (Constants::kWeaponSpinNodeName) con el mismo mecanismo
	// (NiMatrix3::MakeRotation, compuesto sobre la rotación base con la
	// que se quedó clavada -- ver Animation::TickShudder) -- no toca el
	// ángulo lógico de TESObjectREFR ni Havok, así que no afecta a la
	// colisión. 15°, valor dado por el usuario (bajado de 35° -> 20° -> 15°
	// tras varias rondas de prueba en el juego).
	inline constexpr float kStickShudderMaxAngle = 0.261799f;  // rad (15°)

	// Fracción de kStickShudderMaxAngle que la envolvente de amplitud
	// alcanza justo al final de kStickShudderDuration (crecimiento
	// exponencial, nunca llega al 100% exacto de un máximo asintótico) --
	// determina la velocidad de la curva de crecimiento en forma cerrada
	// (ver Animation::TickShudder), no acumulada tick a tick. Placeholder,
	// pendiente de ajustar en el juego: más cerca de 1 hace que el ángulo
	// máximo se alcance más tarde (crecimiento más suave), más lejos de 1
	// lo alcanza antes (crecimiento más brusco).
	inline constexpr float kStickShudderAmplitudeRampFraction = 0.95f;

	// Frecuencia (Hz) de la oscilación al empezar y al terminar el
	// temblor -- sube de forma lineal a lo largo de kStickShudderDuration
	// (chirp de fase continua, misma filosofía de forma cerrada que
	// Throw::ComputeGravityDrop, no acumulada tick a tick). Bajadas ambas
	// (6/30 Hz -> 4/20 Hz -> 3/15 Hz) a petición del usuario tras varias
	// rondas de prueba en el juego, misma proporción entre inicio y fin.
	inline constexpr float kStickShudderFrequencyStart = 3.0f;  // Hz
	inline constexpr float kStickShudderFrequencyEnd = 15.0f;   // Hz

	// Eje local (unitario, ver aviso de kSpinAxisLocal sobre
	// NiMatrix3::MakeRotation) sobre el que oscila el temblor -- distinto
	// del eje de giro en vuelo (kSpinAxisLocal) a propósito, para que el
	// tirón se note como un eje de vibración distinto en vez de una mera
	// versión lenta del giro. Placeholder, pendiente de ajustar en el
	// juego.
	inline constexpr RE::NiPoint3 kStickShudderAxisLocal{ 1.0f, 0.0f, 0.0f };

	// -- Sonido de lanzamiento/vuelo/atrape (12.- AUDIO) --
	// Comprobado en el juego: tres mecanismos distintos de resolución por
	// EditorID (BSAudioManager::GetSoundHandleByName, TESForm::LookupByEditorID
	// -- la tabla global de EditorID del motor, tipada y sin tipar -- y
	// recorrer a mano TESDataHandler::GetFormArray<T>() comparando
	// GetFormEditorID()) fallan igual para los Sound Marker/Sound Descriptor
	// de este proyecto, aunque el registro exista, esté guardado y el
	// plugin esté activo (confirmado: RE::PlaySound, que resuelve por otra
	// vía interna, sí los reproduce). En vez de insistir con EditorID, se
	// resuelve por FormID local + nombre de plugin
	// (RE::TESDataHandler::LookupForm<T>, verificado en TESDataHandler.h,
	// ver Audio::ResolveSoundDescriptor en 12.- AUDIO/SoundResolver.cpp) --
	// mecanismo distinto, ya verificado y en uso en este proyecto para
	// resolver formularios por FormID (WeaponManager::RecoverOrReset). No
	// vale un FormID absoluto fijo porque este plugin no es un master que
	// siempre cargue en el índice 0 (a diferencia de Skyrim.esm) -- de ahí
	// necesitar tanto el nombre del plugin como el FormID *local* (el que
	// se ve en xEdit sin el byte de índice de carga).
	inline constexpr std::string_view kSoundPluginName = "ThorMjolnir.esp";

	// FormID local (visto en xEdit, sin el byte de índice de carga) del
	// Sound Descriptor/Sound Marker del silbido de lanzamiento -- sonado
	// tanto al arrojar el arma (Throw::LaunchWeapon) como al iniciar el
	// tramo de movimiento del regreso (Return::BeginReturnMovement).
	// Placeholder en 0 pendiente de que el usuario lo mire en xEdit -- si
	// no resuelve, Audio::PlaySoundOneShot simplemente avisa por log y no
	// suena nada, mismo criterio defensivo que Constants::kEmbeddedParalysisSpell.
	inline constexpr RE::FormID kThrowLaunchSoundLocalFormID = 0x000000;

	// FormID local del Sound Descriptor/Sound Marker en bucle mientras el
	// arma vuela (ida y vuelta) -- el propio Sound Descriptor debe
	// configurarse como bucle en la Creation Kit, el código
	// (Audio::FlightSound) solo lo arranca y lo para. Mismo criterio de
	// placeholder que el anterior.
	inline constexpr RE::FormID kFlightLoopSoundLocalFormID = 0x000000;

	// FormID local del Sound Descriptor del golpe de atrape
	// (CAP_ThorMjolnir_Sound_MjolnirCatch) -- apunta al Descriptor, no al
	// Sound Marker que lo referencia (CAP_ThorMjolnir_MarkSound_MjolnirCatch,
	// sin usar aquí, la indirección por el Marker no hace falta teniendo
	// el FormID del Descriptor). No es el valor "en bruto" que muestra la
	// Creation Kit (0x00EA61) -- ThorMjolnir.esp tiene el flag ESL activo
	// (comprobado leyendo el header del propio .esp), así que el
	// direccionamiento local real en tiempo de ejecución son los 12 bits
	// bajos de ese valor: confirmado en el juego (Audio::ResolveSoundDescriptor
	// probó ambos y solo este resolvió). Ver Audio::CatchSound
	// (12.- AUDIO/CatchSound.cpp) para cómo se usa.
	inline constexpr RE::FormID kCatchImpactSoundLocalFormID = 0x000A61;

	// Instante (segundos) dentro del propio archivo de audio en el que cae
	// el golpe/impacto real del sonido de atrape -- dado por el usuario
	// (medido a oído/forma de onda). Es el presupuesto de "tiempo de clip"
	// que Audio::CatchSound tiene disponible antes de tener que haber
	// llegado al golpe grabado; ver ese archivo para cómo ajusta la
	// velocidad de reproducción cada tick para que ese presupuesto encaje
	// siempre en el tiempo real que quede hasta el atrape, sea cual sea la
	// duración real del regreso (ya no depende de una predicción fija, así
	// que no hace falta reajustar este valor si cambia la velocidad del
	// regreso).
	inline constexpr float kCatchImpactSoundLeadTime = 1.1270f;

	// Límites de la velocidad de reproducción (RE::BSSoundHandle::SetFrequency,
	// 1.0 = velocidad/tono normal, asumido por convención habitual de
	// motores de audio -- sin documentar en commonlibsse-ng) que
	// Audio::CatchSound puede aplicar para comprimir o estirar su preámbulo
	// y que el golpe grabado siga cayendo justo al atrapar. Acotado para
	// evitar un tono absurdamente agudo/grave en lanzamientos extremos
	// (muy cerca o con el jugador alejándose deprisa) -- placeholders,
	// pendientes de ajustar en el juego.
	inline constexpr float kCatchSoundMinFrequency = 0.5f;
	inline constexpr float kCatchSoundMaxFrequency = 3.0f;

	// Flags de RE::BSAudioManager::GetSoundHandle -- sin significado
	// documentado en commonlibsse-ng (ver BSAudioManager.h). El valor por
	// defecto del propio header (0x1A) resuelve el handle con éxito
	// (GetSoundHandle/Play devuelven true) pero no llega a sonar --
	// confirmado en el juego que el Sound Descriptor en sí está bien (CK:
	// el propio botón "Play" de Auditioning lo reproduce). Probando 0 en
	// su lugar -- sin ninguna documentación de qué bit hace qué, es un
	// ensayo empírico, no una corrección basada en evidencia de código.
	inline constexpr std::uint32_t kFlightSoundHandleFlags = 0x0;

	// Volumen explícito aplicado a todo RE::BSSoundHandle antes de
	// Play() (ver 12.- AUDIO/FlightSound.cpp, CatchSound.cpp) -- un
	// handle recién obtenido de GetSoundHandle no tiene garantizado
	// arrancar a volumen audible por defecto (sin documentar en
	// commonlibsse-ng), y RE::PlaySound -- que sí sonaba, confirmando que
	// el Sound Descriptor en sí está bien -- pasa por una rutina interna
	// distinta que probablemente sí lo fija. 1.0 = volumen máximo sin
	// atenuar, antes de cualquier atenuación por distancia/categoría que
	// aplique el propio motor.
	inline constexpr float kSoundHandleVolume = 1.0f;
}
