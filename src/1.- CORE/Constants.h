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
	inline constexpr const char* kInputConfigPath = "Data/SKSE/Plugins/ThorMjolnir.ini";

	// -- Lanzar: sustitución de animación vía Open Animation Replacer (Fase 3,
	// ver _reference/PLAN-OAR.md) --

	// EditorID de un TESGlobal real de la Creation Kit (no una graph
	// variable -- ver CLAUDE.md, 2026-08-05: sustituye a
	// BehaviorDataInjector, que solo hacía falta para registrar
	// almacenamiento de graph variables, no de Globals) que gatea, en el
	// submod de OAR, la sustitución del ataque ligero de pie por Throw.hkx
	// -- puesto a 1 justo antes de disparar kLightAttackAnimationEvent, a 0
	// en cuanto llega la anotación de liberación (o si el ciclo se aborta
	// por otra vía, p. ej. una pantalla de carga). Prefijado con el nombre
	// del proyecto para evitar colisión de EditorID con otros mods, mismo
	// motivo que "SkipEquipAnimation" en CLAUDE.md para las graph variables.
	inline constexpr const char* kThrowTriggerGlobalEditorID = "CAP_GlobalVariable_ThorMjolnir_ThrowTrigger";

	// Variable vanilla (no inventada por el plugin, ya existe en la tabla de
	// variables de 1hm_behavior.hkx -- no necesita BehaviorDataInjector).
	// Prueba (2026-07-29, ver _reference/PLAN-OAR.md): activada durante
	// State::kThrowing junto con Input::SetMovementLocked, a ver si evita
	// que el motor interprete el ataque como power attack direccional
	// cuando el jugador ya llevaba movimiento/momentum en el instante de
	// soltar el botón (el bloqueo de input nuevo por sí solo no lo evita,
	// comprobado en el juego con el Animation Event Log de OAR). Reutilizada
	// tal cual durante State::kCalling, mismo motivo.
	inline constexpr const char* kAnimationDrivenGraphVariable = "bAnimationDriven";

	// Evento vanilla ya existente en 1hm_behavior.hkx (no uno nuevo) que el
	// submod de OAR intercepta para sustituir el ataque ligero de pie por el
	// clip que corresponda -- Throw.hkx (kThrowTriggerGlobalEditorID) o
	// Call.hkx (kCallTriggerGlobalEditorID) según cuál de las dos esté activa
	// en cada momento (nunca las dos a la vez, el propio WeaponManager las
	// gestiona como mutuamente excluyentes). Ver _reference/PLAN-OAR.md.
	inline constexpr const char* kLightAttackAnimationEvent = "attackStart";

	// Experimento 2026-08-05: con la opción de OAR "Only use triggers from
	// annotations" activada en los tres submods (Throw/Call/Catch --
	// ignora los triggers horneados en el clip vainilla sustituido, solo se
	// procesan los que vengan de anotaciones dentro de nuestro propio
	// archivo, ver el tooltip real de OAR: "The 'Don't convert annotations
	// to triggers' flag is still respected, so make sure to enable the
	// above setting if necessary" -- de ahí que el submod también active
	// "Ignore 'Don't Convert Annotations To Triggers' flag"), Llamada y
	// Atrape se quedan congelados sin salir de AttackRight_State (Lanzar no,
	// porque su propio desequipado real del arma parece forzar un reset del
	// grafo por fuera de cualquier transición). Rastreadas las 18
	// transiciones reales de AttackRight_State en 1hm_behavior.xml (11
	// locales + 7 wildcard) -- ninguna es un simple "ataque terminado,
	// vuelve a idle": todas encadenan a otro ataque (power attacks/combos),
	// condicionadas a ventanas de tiempo ancladas a anotaciones del clip
	// vainilla original (AttackWinStart/AttackWinEnd/weaponSwing) que
	// nuestros clips no llevan -- añadirlas solo reabriría el sistema de
	// combos vainilla, no soluciona el atasco. "attackStop" (evento
	// contrario a kLightAttackAnimationEvent, ya existe en la tabla vanilla)
	// se dispara a mano desde WeaponManager::OnCallReleaseAnimationEvent/
	// OnCatchReleaseAnimationEvent a ver si desatasca el grafo por una vía
	// no rastreada todavía en el XML. Sin confirmar en el juego.
	inline constexpr const char* kAttackStopAnimationEvent = "attackStop";

	// La detección de la anotación de liberación usa la API de Functions de
	// Open Animation Replacer para plugins externos (ver CLAUDE.md para el
	// porqué frente a otros mecanismos descartados, y ver
	// 10.- EVENTS/OARFunctions.h/.cpp y los headers vendorizados en
	// 13.- EXTERNAL/OpenAnimationReplacer/) -- nuestro propio código se
	// registra en OAR con una función custom, y OAR nos llama a nosotros
	// directamente (Run/RunImpl, misma pila de llamada) en el instante
	// exacto de la anotación, sin ningún evento de por medio. El nombre de
	// la anotación en el clip (p. ej. "OAR.MjolnirThrow") solo importa para
	// el "trigger" (par event/payload) que cada config.json liga a nuestra
	// función -- ya no hay ninguna constante aquí que comparar contra un
	// BSAnimationGraphEvent::tag.

	// Red de seguridad: si la anotación real nunca llega (OAR
	// desinstalado/desactualizado/sin la API de Functions, o el ataque se
	// interrumpe por algún motivo), el arma se lanza igualmente pasado este
	// margen en vez de quedarse atascada en State::kThrowing para siempre --
	// el lanzamiento físico debe ocurrir siempre, la sincronía visual es
	// best-effort (decisión del usuario, 2026-07-29). Mayor que el instante
	// de liberación real (0.8s) pero sin llegar a la duración completa del
	// clip.
	inline constexpr std::chrono::milliseconds kThrowReleaseFallbackWindow{ 1500 };

	// -- Llamada: sustitución de animación vía Open Animation Replacer,
	// mismo patrón que Lanzar --

	// EditorID de un TESGlobal real que gatea, en el submod de OAR, la
	// sustitución del mismo ataque ligero de pie por Call.hkx -- mismo
	// motivo que kThrowTriggerGlobalEditorID.
	inline constexpr const char* kCallTriggerGlobalEditorID = "CAP_GlobalVariable_ThorMjolnir_CallTrigger";

	// Experimento (sustituye al arma señuelo/EquipGestureWeapon, rechazado
	// por el usuario: ~500ms de espera visible con el arma real equipada en
	// pose de cuerpo a cuerpo mientras tanto). "iRightHandType" es una
	// graph variable vanilla (Int, no propia, no necesita
	// BehaviorDataInjector) que decide qué rama de combate usa el grafo
	// según el tipo de arma en la mano derecha -- confirmada como variable
	// real documentada públicamente por la comunidad de modding (usada por
	// el propio motor para elegir el set de animaciones), aunque su mapeo
	// exacto de valores no está confirmado con certeza en fuentes públicas.
	// La idea: escribirla directamente a un valor de "arma de una mano" sin
	// pasar por RE::ActorEquipManager en absoluto -- si el grafo respeta el
	// valor sin más, el cambio de rama sería instantáneo y sin equipar nada
	// de verdad (nunca visible, nada que "asentar").
	inline constexpr const char* kRightHandTypeGraphVariable = "iRightHandType";

	// Valor confirmado en el juego (log de diagnóstico en
	// WeaponManager::BeginThrowAnimation, con el arma real en mano): esta
	// partida en concreto lee iRightHandType = 3, no el 1 que se probó
	// primero por analogía con RE::Actor::GetEquippedItemType() (espada a
	// una mano) -- el ataque ligero básico parece rutear igual por
	// 1HM_AttackRight sea cual sea el subtipo de arma a una mano (probado
	// en el juego con el valor 1 antes de corregirlo), pero se deja el
	// valor real confirmado en vez de depender de esa coincidencia.
	inline constexpr std::int32_t kRightHandTypeOneHanded = 3;

	// Mismo mecanismo que Lanzar (ver el bloque de comentarios sobre
	// kThrowReleaseFallbackWindow: API de Functions de Open Animation
	// Replacer). Antes de esto, un primer intento con un evento SoundPlay
	// vanilla nativo se descartó (no llegaba a dispararse, Call.hkx
	// reproducía la animación vanilla en su lugar en vez de la sustituida
	// por OAR -- sin confirmar todavía si el motivo era el propio SoundPlay
	// o la sustitución de OAR en sí).

	// Red de seguridad análoga a kThrowReleaseFallbackWindow, por si la
	// anotación real nunca llega -- mismos motivos (OAR
	// desinstalado/desactualizado/sin la API de Functions, o el ataque se
	// interrumpe). Constante propia (no reutiliza kThrowReleaseFallbackWindow)
	// porque Call.hkx no tiene por qué durar lo mismo que Throw.hkx hasta el
	// chasquido -- placeholder, sin calibrar en el juego.
	inline constexpr std::chrono::milliseconds kCallReleaseFallbackWindow{ 1500 };

	// Sonido del chasquido de dedos, disparado desde el propio código (ya no
	// vía SoundPlay vanilla) en el mismo instante que la anotación de
	// liberación de Llamada (Events::OARFunctions::CallReleaseFunction) --
	// FormID local dado por el usuario desde xEdit (0x01011579, el byte alto
	// 01 es el índice de carga del plugin en su partida, no parte del FormID
	// local). "MarkSound" en el EditorID sugiere un Sound Marker
	// (RE::TESSound), no un Sound Descriptor directo -- Audio::ResolveSoundDescriptor
	// ya resuelve ambos tipos indistintamente (ver SoundResolver.h). Se
	// reutiliza Audio::PlayReliableOneShot (movido de CatchSound.cpp a
	// SoundResolver.h/.cpp para compartirlo) -- el mecanismo simple de un
	// solo RE::BSSoundHandle nunca se ha confirmado fiable en el juego
	// (reporta éxito en cada paso pero no llega a sonar), mientras que el
	// mecanismo triple de PlayReliableOneShot sí (ver Constants::kSoundHandleFlags).
	inline constexpr RE::FormID  kCallReleaseSoundLocalFormID = 0x011579;
	inline constexpr const char* kCallReleaseSoundEditorID = "CAP_ThorMjolnir_MarkSound_FingerSnap";

	// -- Atrape: sustitución de animación vía Open Animation Replacer,
	// mismo patrón que Llamada (iRightHandType directo, sin arma señuelo,
	// ver CHANGELOG v1.10.16/v1.10.17) --

	// EditorID de un TESGlobal real que gatea, en el submod de OAR, la
	// sustitución del mismo ataque ligero de pie por Catch.hkx (mismo
	// motivo que kThrowTriggerGlobalEditorID/kCallTriggerGlobalEditorID).
	inline constexpr const char* kCatchTriggerGlobalEditorID = "CAP_GlobalVariable_ThorMjolnir_CatchTrigger";

	// A diferencia de Llamada (que tuvo que probarse con un SoundPlay
	// vanilla primero, ver CHANGELOG), Catch.hkx ya llevaba esta anotación
	// horneada desde el principio (confirmado con `strings` sobre el propio
	// .hkx) -- mismo mecanismo (API de Functions de OAR) que Lanzar/Llamada.

	// Red de seguridad análoga a kCallReleaseFallbackWindow -- placeholder,
	// sin calibrar en el juego.
	inline constexpr std::chrono::milliseconds kCatchReleaseFallbackWindow{ 1500 };

	// Cuánto sigue reproduciéndose Throw.hkx, visualmente, tras la anotación
	// de liberación -- comprobado en el juego: desequipar el arma real
	// (RE::ActorEquipManager::UnequipObject) en ese mismo instante corta el
	// clip a mitad y salta a la pose de desarmado, porque el motor reevalúa
	// el estado de combate en cuanto cambia el arma equipada, sin importar
	// que el clip sea MODE_SINGLE_PLAY (attackStop, que llega casi a la vez
	// que la propia anotación de liberación, está horneado en el grafo a un
	// tiempo fijo -- no señala el final visual real del clip sustituido,
	// mismo motivo por el que HitFrame ignora qué archivo se está
	// reproduciendo de verdad, ver CLAUDE.md). En vez de desequipar en el
	// acto, WeaponManager::ThrowWeapon oculta el arma real
	// (Animation::SetEquippedWeaponHidden) para que la réplica tome el
	// relevo visual de inmediato, y difiere el desequipado real este margen.
	// Placeholder, pendiente de ajustar en el juego contra la duración real
	// de Throw.hkx tras el fotograma de liberación.
	inline constexpr std::chrono::milliseconds kThrowReleaseVisualHoldDuration{ 400 };

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
	// partida redondo pendiente de ajustar en el juego. Subido de 3000 a
	// 3600 a petición del usuario (2026-08-07, junto con
	// kReturnTargetArrivalSpeed más abajo y la bajada de kReturnMaxDuration):
	// el regreso ya no ralentiza su velocidad para sincronizar con la
	// animación de Atrape (ver Return::BeginReturnMovement/CLAUDE.md), así
	// que ida y vuelta podían subirse de ritmo para transmitir más la
	// sensación de un arma pesada y poderosa, en vez de lenta en distancias
	// medias/cortas. Subido otra vez, de 3600 a 4500, a petición del
	// usuario (2026-08-07).
	inline constexpr float kThrowInitialSpeed = 4500.0f;  // u/s, placeholder
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
	// Cambio de criterio sobre el punto 8 original de Mecanica del
	// arma.txt (ya actualizado ahí): decisión tomada con el usuario de que
	// el regreso ya no acelera de forma constante, sino con una
	// aceleración *creciente* (perfil d(t) = a/(n·(n-1)) · t^n, ver
	// kReturnAccelerationExponent y Return::ComputeTraveledDistance) --
	// simula un tirón magnético cada vez más fuerte según se acerca a la
	// mano, en vez de un tirón parejo durante todo el trayecto.
	//
	// Cambio de criterio (2026-08-07, ver CLAUDE.md): el coeficiente "a" ya
	// no es una constante fija (kReturnAcceleration, eliminada) -- con una
	// aceleración fija, la velocidad media de todo el trayecto escala con
	// distancia^(1-1/n) (despeje algebraico de d(T)=distancia con
	// T=duración), es decir que cuanto más corta la distancia, más lenta se
	// ve *en proporción* la vuelta, porque el arma pasa todo el trayecto
	// corto dentro del primer tramo de la rampa sin llegar a coger
	// velocidad -- confirmado por el usuario tras dos rondas de subir el
	// coeficiente fijo (3000->4500->5500) y seguir viéndose lenta en
	// distancias cortas: subir el coeficiente fijo escala la velocidad por
	// igual en todas las distancias, nunca corrige esa desproporción entre
	// cortas y largas.
	//
	// La aceleración ahora se recalcula por distancia (Return::
	// ComputeReturnAcceleration) para que la velocidad justo al llegar a la
	// mano sea siempre kReturnTargetArrivalSpeed, sin importar la distancia
	// recorrida (despeje algebraico de d(T)=distancia y v(T)=
	// kReturnTargetArrivalSpeed a la vez): a = (n-1)/n^(n-1) ·
	// kReturnTargetArrivalSpeed^n / distancia^(n-1), con duración resultante
	// T = n·distancia / kReturnTargetArrivalSpeed (lineal con la distancia,
	// a diferencia de antes). Un regreso corto ahora tarda menos tiempo en
	// llegar (T más pequeño) pero con una aceleración proporcionalmente
	// mayor para llegar exactamente igual de rápido que uno largo, en vez
	// de llegar más lento -- la sensación de "tirón" debería notarse igual
	// de contundente sea cual sea la distancia. kReturnMaxDuration sigue
	// límitando la duración total (contada desde que se desprende del
	// todo, sin el temblor del punto 11) exactamente igual que antes -- ver
	// Return::ComputeReturnAcceleration para el recálculo híbrido si a este
	// ritmo tardaría más (ahí sí se sacrifica la velocidad de llegada
	// constante, a cambio de no superar el límite de duración).
	//
	// ATENCIÓN al tocar esta constante (o kReturnAccelerationExponent/
	// kReturnMaxDuration más abajo): Return::ComputeReturnDuration predice
	// la duración total del regreso a partir de estos mismos valores, y
	// Return::BeginReturn usa esa predicción, una sola vez al empezar todo
	// el regreso, para calcular con antelación el instante en que hay que
	// disparar el sonido de arranque del atrape (Constants::
	// kCatchStartSoundLeadTime, Audio::CatchCue) -- una pequeña
	// desincronización de ese arranque es aceptable (decisión del
	// usuario), pero cambios grandes en la velocidad del regreso sin
	// volver a probar en el juego pueden notarse. Placeholder, pendiente de
	// calibrar en el juego.
	inline constexpr float kReturnTargetArrivalSpeed = 5000.0f;  // u/s
	// Subido de 2.0 a 2.3 a petición del usuario, para dar presupuesto de
	// tiempo extra al suavizado del tramo final (kReturnTailDistance/
	// kReturnTailMinRate más abajo) sin acortar el resto del recorrido --
	// ese suavizado alarga la duración real un poco más allá de lo que
	// predice Return::ComputeReturnDuration (que no lo conoce, ver esa
	// función), así que este límite necesitaba margen de sobra. Bajado de
	// 2.3 a 1.5 a petición del usuario (2026-08-07), a la vez que se
	// introducía kReturnTargetArrivalSpeed -- el arma debe tardar como mucho
	// 1,5s en volver, nunca los ~2s de antes, para reforzar la sensación de
	// un regreso rápido y contundente.
	inline constexpr float kReturnMaxDuration = 1.5f;

	// Exponente del perfil de aceleración creciente de arriba. 2 recupera
	// la aceleración constante de siempre; valores mayores hacen que el
	// arma empiece más despacio y tire cada vez más fuerte cuanto más
	// cerca está de la mano (más "imán", menos "empujón parejo") -- 3
	// sería aceleración que crece en línea recta con el tiempo ("jerk"
	// constante). Placeholder intermedio entre ambos, pendiente de
	// calibrar en el juego.
	inline constexpr float kReturnAccelerationExponent = 2.5f;

	// Suavizado del tramo final de llegada (a petición del usuario, para
	// que el golpe/sonido de atrape no sea abrupto) -- NO invierte el
	// perfil de aceleración creciente del punto 8 (seguiría contradiciendo
	// el documento de diseño), solo lo atenúa en el último tramo: una vez
	// la distancia a la mano cae por debajo de kReturnTailDistance, el
	// tiempo que avanza Return::BeginReturnMovement hacia
	// Return::ComputeTraveledDistance se escala por un factor que baja
	// suavemente (curva suave tipo smoothstep, no un corte lineal brusco)
	// de 1.0 (velocidad de tiempo normal, fuera del tramo final) a
	// kReturnTailMinRate (justo al llegar) -- el arma sigue acelerando
	// "de imán" según su propio perfil, pero ese perfil avanza más
	// despacio en tiempo real en el último tramo, dando la sensación de
	// desaceleración hacia la mano sin tocar la fórmula física en sí.
	// El golpe final del atrape (Audio::CatchCue::PlayEnd) se dispara
	// siempre en el instante real detectado de la llegada, así que este
	// suavizado no lo afecta a él -- lo que sí suaviza es la propia
	// llegada física (menos brusca visualmente) y da algo más de tiempo
	// real de sobra antes de ese instante. Placeholders sin calibrar en
	// el juego.
	inline constexpr float kReturnTailDistance = 300.0f;
	inline constexpr float kReturnTailMinRate = 0.35f;

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
	// cerrar el ángulo general -- y vueltos a subir (2026-08-07, a
	// petición del usuario) porque a ese rango cerrado la curva resultaba
	// apenas perceptible, sobre todo ahora que el regreso vuela más rápido
	// (kReturnTargetArrivalSpeed más arriba): a más velocidad, menos tiempo
	// real para apreciar la misma desviación geométrica, así que hacía
	// falta una desviación mayor para que siguiera notándose. Con este
	// rango la fracción vuelve a acercarse al valor fijo original, pero el
	// tope absoluto (kReturnCurveMaxOffset) se deja por debajo de aquel
	// (350 en vez de 500) para no reabrir del todo el problema original
	// (demasiado amplia) en distancias largas.
	inline constexpr float kReturnCurveLateralFractionMin = 0.20f;
	inline constexpr float kReturnCurveLateralFractionMax = 0.30f;
	inline constexpr float kReturnCurveMinOffset = 70.0f;
	inline constexpr float kReturnCurveMaxOffset = 350.0f;

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

	// Punto 10 (segunda mitad): "justo antes de... volver a la mano del
	// jugador, se endereza para que... el mango quede orientado para que
	// el jugador pueda agarrarla" -- ver Animation::TickSpinStraighten,
	// llamada desde Return::BeginReturnMovement en vez de
	// Animation::TickSpin durante los últimos instantes del regreso.
	//
	// Cambio de criterio (2026-08-07, ver CLAUDE.md): esta ventana ya NO
	// arranca en el mismo instante que Return::ReturnCallbacks::onApproaching
	// (Constants::kCatchAnimationLeadTime, 0,5s antes de la llegada real,
	// atado a la duración de Catch.hkx -- eso no cambia, sigue disparando
	// el gesto de Atrape a esa distancia temporal fija) -- son dos
	// disparadores independientes ahora. Antes, al compartir instante con
	// onApproaching, la ventana de 0,5s consumía la mayor parte (o la
	// totalidad) de un regreso corto o medio, y el arma apenas llegaba a
	// girar antes de empezar a frenar el giro (bug reportado por el
	// usuario, 2026-08-07: "el enderezado se produce desde muy lejos,
	// haciendo que el arma no gire en regresos cortos o medios"). Esta
	// constante marca en su lugar cuánto antes de la llegada real empieza
	// a verse el enderezado en sí -- deliberadamente mucho más corta que
	// kCatchAnimationLeadTime, para que el giro (Animation::TickSpin) siga
	// ocupando la mayor parte del trayecto y el enderezado solo se note ya
	// bastante al final. Placeholder, pendiente de calibrar en el juego.
	inline constexpr float kSpinStraightenLeadTime = 0.2f;  // s, placeholder

	// Segunda mitad del punto 10, caso "impacto" ("justo antes de alcanzar
	// un objetivo... se endereza para que el filo/cabeza apunte hacia el
	// objetivo"): se probaron dos versiones -- una duración fija (0.15s) y
	// después una derivada del ángulo a corregir a velocidad angular
	// constante -- y ninguna convenció al usuario. El ángulo real a
	// corregir depende de en qué fase del giro continuo
	// (Animation::TickSpin) iba el arma justo al impactar, básicamente al
	// azar entre 0 y 180 grados según cuánto llevara volando: con
	// cualquiera de las dos versiones, la posición final variaba de forma
	// poco natural entre lanzamientos, y con la segunda, las correcciones
	// pequeñas (más probables en lanzamientos cortos/medios, con menos
	// giro acumulado) se veían como un cambio de orientación instantáneo
	// en vez de una animación gradual. Decisión del usuario (2026-08-08):
	// eliminado el enderezado al clavarse por completo -- el arma se
	// queda congelada en el ángulo de vuelo arbitrario que tuviera en el
	// instante del impacto (Throw::LaunchWeapon / Combat::BeginEmbeddedEffect
	// ya no llaman a Animation::TickSpinStraighten en ese punto). El caso
	// "vuelta a la mano del jugador" (kSpinStraightenLeadTime arriba) no
	// se ha tocado, sigue igual.

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
	// Duración *mínima* de la vibración antes de desprenderse al iniciar
	// el regreso desde un objetivo clavado. Mecanica del arma.txt da 0,1s
	// explícitamente, pero a ese valor el usuario no apreciaba el efecto en
	// el juego (probablemente por el bug de Update3DPosition corregido en
	// Animation::TickShudder/Return::BeginReturn -- ver CHANGELOG.md, no
	// por la duración en sí) -- subido a 0,5s a petición expresa para
	// confirmar visualmente que el temblor ocurre antes de recortarlo de
	// vuelta hacia el valor del documento.
	//
	// Cambio de criterio (2026-08-07, ver CLAUDE.md y Return::BeginReturn):
	// esta constante ya no es la duración fija del temblor, sino su suelo.
	// Return::BeginReturn puede alargarla en distancias cortas, donde el
	// vuelo de vuelta (ya a velocidad natural, sin ralentizar -- ver
	// Return::BeginReturnMovement) no dejaría tiempo suficiente para que la
	// animación de Atrape se sincronice con la llegada real; antes era el
	// propio vuelo el que se ralentizaba para cubrir ese hueco, ahora es
	// este temblor el que se estira (Animation::TickShudder acepta la
	// duración real como parámetro en vez de asumir siempre esta
	// constante). Nunca se acorta por debajo de este valor.
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

	// -- Sonido de lanzamiento/atrape (12.- AUDIO) --
	// Resolución por FormID local + nombre de plugin
	// (RE::TESDataHandler::LookupForm<T>, verificado en TESDataHandler.h,
	// ver Audio::ResolveSoundDescriptor en 12.- AUDIO/SoundResolver.cpp) --
	// TESForm::LookupByEditorID (la tabla global de EditorID del motor) y
	// BSAudioManager::GetSoundHandleByName no encuentran los Sound Marker/
	// Sound Descriptor de este proyecto aunque el registro exista, esté
	// guardado y el plugin esté activo (comprobado en el juego); el FormID
	// local sí resuelve de forma fiable. No vale un FormID absoluto fijo
	// porque este plugin no es un master que siempre cargue en el índice 0
	// (a diferencia de Skyrim.esm) -- de ahí necesitar tanto el nombre del
	// plugin como el FormID *local* (el que se ve en xEdit sin el byte de
	// índice de carga).
	//
	// "ThorMjolnirOAR.esp", no "ThorMjolnir.esp" -- exclusivo de esta rama
	// (oar): la copia del mod para probar OAR en paralelo a "behavior" quedó
	// con el ESP renombrado (misma carpeta de mod duplicada, ver
	// D:\Modlists\SME\mods\ThorMjolnir_OAR\ThorMjolnirOAR.esp), así que
	// RE::TESDataHandler::LookupForm fallaba en silencio para *todos* los
	// FormID (incluidos los ya confirmados funcionando antes) -- el nombre
	// de plugin no coincidía con ninguno realmente activo en la partida.
	inline constexpr std::string_view kSoundPluginName = "ThorMjolnirOAR.esp";

	// FormID local (visto en xEdit, sin el byte de índice de carga) del
	// Sound Marker del silbido de lanzamiento -- sonado tanto al arrojar el
	// arma (Throw::LaunchWeapon) como al iniciar el tramo de movimiento del
	// regreso (Return::BeginReturnMovement). Dado por el usuario como
	// 0x01014092 (visto en xEdit/CK, con el byte de índice de carga
	// incluido) -- `ThorMjolnirOAR.esp` tiene el flag ESL activo (ver
	// CLAUDE.md, "Errores comunes a vigilar"), así que el valor real de 12
	// bits se obtiene enmascarando los últimos 3 dígitos hex
	// (0x01014092 & 0xFFF = 0x092), independientemente de si se enmascara
	// el valor completo o solo su parte local de 6 dígitos (0x014092) --
	// el resultado es el mismo, el índice de carga cae siempre fuera de
	// los 12 bits bajos.
	inline constexpr RE::FormID kThrowLaunchSoundLocalFormID = 0x092;

	// EditorID del mismo Sound Marker, dado por el usuario -- necesario
	// para el RE::PlaySound de refuerzo de Audio::PlayReliableOneShot (ver
	// SoundResolver.h). Cambio de criterio (2026-08-08, ver CLAUDE.md): el
	// silbido de lanzamiento usaba Audio::PlaySoundOneShot (ya retirada),
	// el mecanismo más simple de un solo RE::BSSoundHandle -- confirmado
	// en el juego que, igual que ya pasaba con los sonidos de Atrape/
	// Llamada antes de este mismo cambio, GetSoundHandle/FadeInPlay()
	// reportaban éxito en el log pero no sonaba nada. Movido al mecanismo
	// triple ya confirmado fiable para esos otros dos.
	inline constexpr const char* kThrowLaunchSoundEditorID = "CAP_ThorMjolnir_Sound_MjolnirThrow";

	// -- Sonido de atrape, en dos partes (12.- AUDIO/CatchSound) --
	// Rediseño completo a petición del usuario, sustituyendo por completo
	// el diseño anterior de un único sonido con ajuste continuo de
	// velocidad de reproducción (Audio::CatchSound + RE::BSSoundHandle::
	// SetFrequency cada tick, ver CHANGELOG.md para el porqué se abandonó):
	// en vez de estirar/comprimir un único clip para que su golpe grabado
	// caiga siempre justo en el instante de la llegada, el sonido se
	// divide en dos Sound Descriptor independientes --
	// CAP_ThorMjolnir_Sound_MjolnirCatch_Start (arranque, sonado con
	// antelación; una pequeña desincronización de este arranque es
	// aceptable, nunca perfecta) y CAP_ThorMjolnir_Sound_MjolnirCatch_End
	// (golpe final, disparado siempre exactamente en el instante real
	// detectado de la llegada, sin depender de ningún cálculo ni de que el
	// arranque haya sonado). Ver Audio::CatchCue
	// (12.- AUDIO/CatchSound.h/.cpp).
	//
	// FormID local de cada Sound Descriptor, dados por el usuario tal cual
	// los muestra xEdit (byte de índice de carga "01" ya excluido). Sin
	// enmascarar a 12 bits a propósito -- ThorMjolnir.esp tiene el flag
	// ESL activo, así que Audio::ResolveSoundDescriptor reintenta solo con
	// esa máscara si el valor en bruto no resuelve (ver ese archivo); se
	// deja así en vez de hardcodear ya el valor enmascarado porque, a
	// diferencia de kCatchStartSoundLocalFormID (mismo registro que el
	// antiguo sonido de atrape único, ya confirmado en el juego que
	// 0x00EA61 enmascara a 0x000A61), kCatchEndSoundLocalFormID es un
	// registro nuevo sin confirmar todavía.
	inline constexpr RE::FormID kCatchStartSoundLocalFormID = 0x00EA61;
	inline constexpr RE::FormID kCatchEndSoundLocalFormID = 0x00F527;

	// EditorID de cada Sound Descriptor -- no un std::string_view como
	// kSoundPluginName porque RE::PlaySound(const char*) exige una cadena
	// terminada en nulo (ver el porqué de esta llamada en el comentario de
	// kSoundHandleFlags más abajo).
	inline constexpr const char* kCatchStartSoundEditorID = "CAP_ThorMjolnir_Sound_MjolnirCatch_Start";
	inline constexpr const char* kCatchEndSoundEditorID = "CAP_ThorMjolnir_Sound_MjolnirCatch_End";

	// Segundos antes del instante real de llegada en los que debe sonar el
	// arranque -- dado por el usuario (medido a oído). Return::BeginReturn
	// calcula, una sola vez al empezar todo el regreso -- temblor de
	// desprendimiento incluido si el arma estaba clavada
	// (Constants::kStickShudderDuration, tenido en cuenta explícitamente a
	// petición del usuario, no solo el tramo de movimiento) -- el retardo
	// desde ese instante hasta que toca disparar el arranque; ver
	// Audio::CatchCue::UpdateStart.
	inline constexpr float kCatchStartSoundLeadTime = 1.066f;

	// Cuánto tarda Catch.hkx, desde que arranca, en llegar a su propia
	// anotación de liberación PIE.ThorMjolnirCatch (que gatilla el
	// reequipado real, WeaponManager::OnCatchReleaseAnimationEvent) --
	// medido por el usuario directamente sobre el clip: 30 fotogramas a
	// 30 FPS de duración total, anotación en el fotograma 15 -> 0.5s. La
	// sincronización con la llegada física real del arma (ver
	// Return::BeginReturn) se apoya en este valor exacto -- ver también
	// Constants::kMinCatchAnimationDelay para el otro extremo del cálculo
	// (cuándo puede empezar a reproducirse Catch.hkx, no cuánto dura).
	inline constexpr float kCatchAnimationLeadTime = 0.5f;

	// Margen de seguridad añadido al umbral de disparo de onApproaching
	// (Return::BeginReturnMovement: dispara cuando el tiempo real restante
	// simulado cae a kCatchAnimationLeadTime + este margen, no exactamente
	// kCatchAnimationLeadTime). Diagnosticado en el juego (2026-08-08, ver
	// CLAUDE.md): con log de diagnóstico activado, la estimación por
	// simulación (Return::SimulateRemainingReturnTime) predecía el tiempo
	// restante con un error de solo unos pocos milisegundos frente a la
	// duración real de Catch.hkx hasta su propia anotación (p. ej.
	// estimado 0.496s vs. los 0.5s reales del clip) -- suficientemente
	// exacta en la mayoría de casos, pero ese margen de pocos milisegundos
	// es más estrecho que un solo tick (Constants::kTickDeltaSeconds,
	// ~16ms) y que el jitter real del hilo-que-duerme-y-reencola del
	// bucle de tick: en distancias medias/largas, la anotación real de
	// Catch.hkx (con temporización fija) y la detección interna de
	// llegada física (Return::BeginReturnMovement, que depende de que el
	// bucle ejecute un tick más) competían por quién llegaba primero, y la
	// anotación ganaba esa carrera con la frecuencia suficiente como para
	// que Audio::CatchCue::PlayEnd casi nunca llegara a dispararse (bug
	// reportado por el usuario) -- confirmado con logs reales: en varios
	// casos, "la réplica ha llegado a la mano" nunca llegaba a aparecer
	// antes de que WeaponManager::OnCatchReleaseAnimationEvent cancelara
	// el bucle desde fuera. Este margen adelanta el disparo de
	// onApproaching lo suficiente para que la llegada física gane esa
	// carrera con margen de sobra, a costa de un desajuste igual de
	// pequeño (y ya aceptado de antemano, ver el resto de este archivo)
	// entre el inicio de Catch.hkx y la llegada real. Placeholder,
	// pendiente de calibrar en el juego si hiciera falta más margen.
	inline constexpr float kCatchApproachSafetyMargin = 0.1f;

	// Cuánto sigue reproduciéndose Call.hkx, en tiempo real, después de su
	// propia anotación de liberación (la que dispara
	// WeaponManager::OnCallReleaseAnimationEvent y arranca el regreso
	// físico) hasta que el propio clip termina del todo -- medido por el
	// usuario: 25 fotogramas a 30 FPS de duración total, anotación en el
	// fotograma 10 -> (25-10)/30 = 0.5s de cola. Bug reportado por el
	// usuario (2026-08-03): sin este margen, con el arma muy cerca al
	// llamarla, WeaponManager::BeginCatchAnimation (con su propio
	// player->NotifyAnimationGraph("attackStart")) podía dispararse casi
	// en el mismo instante en que Call.hkx seguía resolviéndose -- dos
	// disparos de ese mismo evento vanilla demasiado seguidos confundían
	// al grafo de forma no determinista (a veces no arrancaba ninguna
	// animación, a veces repetía Call.hkx, a veces caía en un ataque
	// cuerpo a cuerpo). Return::BeginReturn usa este valor junto con
	// kCatchAnimationLeadTime para, si hace falta, alargar el temblor de
	// desprendimiento del punto 11 (Constants::kStickShudderDuration como
	// suelo, nunca el vuelo de regreso en sí -- cambio de criterio
	// 2026-08-07, ver CLAUDE.md) cuando la distancia es tan corta que la
	// física natural no dejaría tiempo para ninguno de los dos márgenes --
	// nunca desacoplando animación y física con temporizadores
	// independientes (a petición del usuario: la sincronización no es
	// negociable).
	inline constexpr float kMinCatchAnimationDelay = 0.5f;

	// Cuánto sigue reproduciéndose Call.hkx/Catch.hkx, en tiempo real,
	// después de su propia anotación de liberación hasta que el propio
	// clip termina del todo -- mismos 0.5s de cola ya medidos y descritos
	// en el comentario de kMinCatchAnimationDelay (Call, 25 fotogramas a
	// 30 FPS, anotación en el 10) y en el de kCatchAnimationLeadTime más
	// arriba (Catch, 30 fotogramas, anotación en el 15) respectivamente --
	// constantes propias en vez de reutilizar esas directamente (aunque
	// coincidan en valor) porque describen un concepto distinto: cuánto
	// hay que ESPERAR después de la anotación antes de desatascar el grafo
	// (WeaponManager::FinishCallAnimation/FinishCatchAnimation), no cuándo
	// puede dispararse la propia anotación ni cuánto debe esperar Return
	// para la sincronización física.
	//
	// Cambio de criterio (2026-08-08, a petición del usuario): antes,
	// WeaponManager::OnCallReleaseAnimationEvent/OnCatchReleaseAnimationEvent
	// disparaban Constants::kAttackStopAnimationEvent en el mismo instante
	// que la propia anotación de liberación -- necesario para el
	// reequipado/inicio del regreso físico (deben ocurrir exactamente ahí,
	// eso no cambia), pero cortaba el clip a mitad de esta cola, que nunca
	// llegaba a reproducirse (bug reportado por el usuario: "la animación
	// de Atrape/Llamada queda cortada, no se reproduce entera"). Ahora
	// attackStop (y el resto de la limpieza del grafo -- bloqueo de
	// movimiento, AnimationDriven, el propio trigger de OAR) se difiere
	// este margen, dejando que la cola visual del clip termine de verdad
	// antes de desatascar el grafo -- mismo patrón que
	// Throw::ThrowWeapon ya usa para su propio hueco
	// (Constants::kThrowReleaseVisualHoldDuration), que no necesita este
	// cambio porque no dispara attackStop en absoluto (ver CLAUDE.md).
	//
	// std::chrono::milliseconds, no float como kCatchAnimationLeadTime/
	// kMinCatchAnimationDelay -- a diferencia de esas dos (comparadas
	// contra tiempos calculados en coma flotante dentro de 5.- RETURN),
	// estas dos solo se usan como argumento de std::this_thread::sleep_for,
	// mismo tipo que el resto de márgenes de espera del proyecto
	// (kThrowReleaseFallbackWindow, kCallReleaseFallbackWindow, etc.).
	//
	// kCatchAnimationTailDuration se deja en los 0,5s completos medidos
	// (funciona bien tal cual, confirmado por el usuario). kCallAnimationTailDuration
	// bajada a propósito por debajo de su cola completa (2026-08-08, a
	// petición del usuario) -- distinto problema, solo en Llamada: si el
	// jugador ya llevaba movimiento al pulsar el botón, Call.hkx "se
	// vuelve a reproducir" y el personaje desliza un poco durante el
	// clip -- mismo síntoma que el power attack direccional vanilla ya
	// documentado (ver kAnimationDrivenGraphVariable), que
	// Animation::SetAnimationDriven no está evitando del todo aquí pese a
	// activarse durante todo State::kCalling. Sin causa raíz confirmada
	// todavía (no se ha determinado si el propio grafo reevalúa el input
	// de movimiento retenido en algún punto concreto de la cola, ni si
	// existe tal punto) -- experimento pedido por el usuario: adelantar
	// attackStop (y el resto de la limpieza, ver FinishCallAnimation) a
	// medio camino de la cola en vez de al final, con la esperanza de que
	// evite lo que sea que dispare la reevaluación, a costa de perder la
	// mitad final de la cola visual del clip (mejor que perderla entera,
	// que era el bug original). Pendiente de confirmar en el juego si esto
	// arregla el deslizamiento, y de recalibrar el valor en cualquier
	// dirección según el resultado.
	inline constexpr std::chrono::milliseconds kCallAnimationTailDuration{ 250 };
	inline constexpr std::chrono::milliseconds kCatchAnimationTailDuration{ 500 };

	// Temblor de cámara al cerrar la mano sobre el arma, disparado en el
	// mismo instante que el reequipado real (WeaponManager::
	// OnCatchReleaseAnimationEvent, misma anotación PIE.ThorMjolnirCatch ya
	// usada para el reequipado/attackStop) -- vía RE::ShakeCamera
	// (RE/M/Misc.h), el mismo motor nativo detrás de Game.ShakeCamera() en
	// Papyrus. No es un punto numerado de "Mecanica del arma.txt" (no cubre
	// cámara en ningún punto) -- puro polish pedido aparte. Placeholders sin
	// valor de referencia previo, pendientes de ajustar en el juego.
	inline constexpr float kCatchShakeStrength = 1.5f;
	inline constexpr float kCatchShakeDuration = 0.2f;

	// -- Zoom de cámara al apuntar --
	// Tampoco es un punto numerado de "Mecanica del arma.txt" (no cubre
	// cámara en ningún punto, igual que kCatchShakeStrength/kCatchShakeDuration
	// arriba) -- mecánica nueva pedida aparte.
	//
	// Historial de dos intentos descartados antes de llegar a esto
	// (2026-08-07, ver CHANGELOG.md para el detalle completo):
	// 1) Escribir RE::ThirdPersonState::targetZoomOffset una sola vez al
	//    activar, bajo la hipótesis (nunca confirmada contra código fuente,
	//    solo inferida del nombre de los campos) de que el motor interpola
	//    currentZoomOffset hacia ahí por su cuenta -- la cámara no paraba de
	//    acercarse mientras se mantenía pulsado el botón.
	// 2) Rampa manual propia escribiendo targetZoomOffset Y
	//    currentZoomOffset a la vez cada tick -- ya no avanzaba infinito,
	//    pero por pequeña que se hiciera la magnitud del offset (-40 a -12,
	//    sin diferencia visible), la cámara en tercera persona atravesaba al
	//    personaje varios METROS por delante suyo, frenada solo por
	//    colisión real contra geometría (muros/vallas). Confirmado con una
	//    prueba A/B (función completamente inerte vs. activa) que el
	//    problema lo causaba justo este código, pese a que un log de
	//    diagnóstico mostraba posOffsetExpected/posOffsetActual (mismo
	//    struct) sin cambios entre zoom activo/inactivo -- esos dos campos
	//    están ligados al sistema de colisión/posicionamiento real de la
	//    cámara en tercera persona de un modo que no se llegó a entender
	//    del todo, y no merece la pena seguir investigándolo.
	//
	// Solución actual: RE::PlayerCamera::RUNTIME_DATA2::worldFOV (ver
	// Animation::SetAimZoom/StartAimZoomRamp, mismo patrón de rampa manual
	// que el intento 2, pero sobre este campo) -- un parámetro de
	// renderizado puro (ángulo de visión), sin relación con la posición ni
	// la colisión de la cámara, así que no hereda ninguno de los dos
	// problemas de arriba. Mismo campo en primera y tercera persona, ya no
	// hace falta distinguir la perspectiva.
	//
	// Offset (no valor absoluto) sobre el FOV que ya hubiera en cada momento
	// (Animation::SetAimZoom guarda el valor previo antes de sumar este
	// offset, y lo restaura tal cual al desactivar). Negativo estrecha el
	// campo de visión (efecto zoom). Placeholder sin calibrar en el juego
	// todavía -- primer valor a ajustar si el efecto resulta de más o de
	// menos.
	inline constexpr float kAimZoomFOVOffset = -15.0f;

	// Duración de la rampa manual de entrada Y de salida (Animation::
	// StartAimZoomRamp) -- a petición del usuario, más corta que lo que
	// tardaba el intento anterior (delegado en el motor, sin control sobre
	// la velocidad real). Placeholder, primer valor a subir/bajar si se ve
	// demasiado brusca o demasiado lenta en el juego.
	inline constexpr float kAimZoomTransitionDuration = 0.2f;  // s, placeholder

	// Flags de RE::BSAudioManager::GetSoundHandle -- sin significado
	// documentado en commonlibsse-ng (ver BSAudioManager.h), 0 sin más
	// justificación que ser el valor neutro (probado también el 0x1A por
	// defecto del propio header, sin diferencia observada). El propio
	// GetSoundHandle/BSSoundHandle::Play() nunca llegaron a sonar en el
	// juego pese a reportar éxito en cada paso, por muchas combinaciones
	// de a_flags/posición/volumen/Output Model que se probaran -- lo que
	// sí funciona, confirmado repetidas veces (ver Audio::CatchCue), es
	// combinar tres cosas a la vez: un RE::BSSoundHandle "de cebado" sin
	// posición dejado sonar por su cuenta (nunca detenido con Stop()),
	// RE::PlaySound(editorID) en el mismo instante, y el
	// RE::BSSoundHandle real arrancado con FadeInPlay(0) en vez de Play().
	// Sin explicación firme de por qué -- documentado como comportamiento
	// empírico confirmado, no como diagnóstico pendiente de limpiar.
	inline constexpr std::uint32_t kSoundHandleFlags = 0x0;

	// Volumen explícito aplicado a todo RE::BSSoundHandle antes de
	// FadeInPlay() (ver 12.- AUDIO/SoundResolver.cpp, CatchSound.cpp) -- un
	// handle recién obtenido de GetSoundHandle no tiene garantizado
	// arrancar a volumen audible por defecto (sin documentar en
	// commonlibsse-ng). 1.0 = volumen máximo sin atenuar, antes de
	// cualquier atenuación por distancia/categoría que aplique el propio
	// motor.
	inline constexpr float kSoundHandleVolume = 1.0f;

	// -- VFX de movimiento (chispas), puro polish sin punto numerado en
	// Mecanica del arma.txt -- activo mientras el arma se mueve de verdad
	// (State::kAiming/kThrowing/kThrown/kCalling/kReturning, a petición del
	// usuario), apagado en reposo (kInHand) o clavada (kStuck). Ver
	// 8.- ANIMATION/WeaponVFX.h/.cpp.
	//
	// Historial de arquitectura (ver CHANGELOG.md para el detalle completo
	// de cada ronda): 1) RE::BSTempEffectParticle::Spawn -- cargaba el
	// modelo de verdad pero nunca renderizó nada; 2) Activator real vía
	// PlaceObjectAtMe + RE::NiNode::AttachChild -- probado incluso con un
	// objeto garantizado bueno (copia del arma equipada), tampoco renderizó
	// nada; 3) Activator real + bucle de tick manual (confirmado que SÍ
	// renderiza -- la prueba con el arma equipada se vio en el juego) pero,
	// con el .nif editado para no depender de "fToggleBlend" (manager y
	// secuencias borrados), tampoco mostraba las chispas ni en el propio
	// preview de la Creation Kit (el vanilla sin tocar sí se ve ahí) --
	// borrar el manager rompe algo que el formato no explica por sí solo.
	//
	// Arquitectura actual: el .nif vuelve a ser una copia vanilla de
	// fxsparkfountaintoggle.nif sin tocar (manager + secuencias partA/partB
	// intactas). En vez de depender del script FXSetBlendVariableScript
	// (Papyrus, solo se ejecuta sobre una referencia real cargada por el
	// motor normal, no aplicable aquí), Animation::StartTicking llama a
	// RE::IAnimationGraphManagerHolder::SetGraphVariableFloat("fToggleBlend", ...)
	// directamente en C++ sobre la referencia recién colocada -- mismo
	// mecanismo que el script, sin pasar por Papyrus. Ahora sí es una
	// referencia real (TESObjectREFR, vía PlaceObjectAtMe) con grafo de
	// animación propio -- a diferencia del primer intento
	// (BSTempEffectParticle, sin grafo), esta llamada es válida.
	//
	// Activator propio, creado por el usuario en la Creation Kit copiando
	// el vanilla FXSparkFountainToggleHeavy: EditorID
	// CAP_ThorMjolnir_Activator_Sparkles, FormID dado por el usuario tal
	// cual en xEdit/CK (0x01014B57) -- ThorMjolnirOAR.esp tiene el flag ESL
	// activo (ver CLAUDE.md), así que se enmascara a 12 bits: 0x01014B57 &
	// 0xFFF = 0xB57.
	inline constexpr RE::FormID kMovementVfxActivatorLocalFormID = 0xB57;

	// Graph variable que el script vanilla (FXSetBlendVariableScript) pone
	// en OnLoad() -- ver arriba. Sin confirmar todavía qué extremo (0.0 o
	// 1.0) corresponde a la variante "Heavy" (más partículas, más rápidas)
	// frente a la más floja -- 1.0 es la primera prueba, a falta de
	// verificarlo en el juego contra las dos.
	inline constexpr const char* kMovementVfxToggleBlendVariable = "fToggleBlend";
	inline constexpr float       kMovementVfxToggleBlendValue = 1.0f;

	inline constexpr float kMovementVfxScale = 1.0f;  // placeholder, pendiente de ajustar en el juego
}
