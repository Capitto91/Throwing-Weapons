// Constantes del plugin: nombres de formularios y nodos, tiempos, distancias y parámetros de los efectos.
// Los ajustes que el usuario puede cambiar están en Settings.

#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

namespace Constants
{
	// EditorID de la Keyword que identifica al arma arrojadiza.
	inline constexpr std::string_view kThrowableWeaponKeyword{ "WAF_ThrowableWeapon" };

	// Ruta del INI, relativa a la carpeta del juego.
	inline constexpr const char* kInputConfigPath = "Data/SKSE/Plugins/ThorMjolnir.ini";

	// -- Lanzar (Open Animation Replacer) --

	// Global que hace que el submod de OAR sustituya el ataque ligero por Throw.hkx.
	// Lo activa WeaponManager antes de kLightAttackAnimationEvent y lo apaga con la liberación.
	inline constexpr const char* kThrowTriggerGlobalEditorID = "CAP_GlobalVariable_ThorMjolnir_ThrowTrigger";

	// Graph variable vanilla activada durante Lanzar y Llamada junto al bloqueo de movimiento.
	inline constexpr const char* kAnimationDrivenGraphVariable = "bAnimationDriven";

	// Evento vanilla que OAR sustituye por Throw.hkx, Call.hkx o Catch.hkx según el Global activo.
	inline constexpr const char* kLightAttackAnimationEvent = "attackStart";

	// Evento vanilla que devuelve el grafo a reposo tras Llamada y Atrape.
	inline constexpr const char* kAttackStopAnimationEvent = "attackStop";

// Red de seguridad: el arma se lanza pasado este margen si no llega la anotación de Throw.hkx.
	inline constexpr std::chrono::milliseconds kThrowReleaseFallbackWindow{ 1500 };

	// -- Llamada (Open Animation Replacer) --

	// Global que hace que el submod de OAR sustituya el ataque ligero por Call.hkx.
	inline constexpr const char* kCallTriggerGlobalEditorID = "CAP_GlobalVariable_ThorMjolnir_CallTrigger";

	// Graph variable vanilla con el tipo de arma de la mano derecha; se escribe para el gesto sin equipar nada.
	inline constexpr const char* kRightHandTypeGraphVariable = "iRightHandType";

	// Valor de iRightHandType para "arma de una mano".
	inline constexpr std::int32_t kRightHandTypeOneHanded = 3;

	// Tipo de animación del arma desenvainada (AttackAnimType): maza, para los ataques de maza.
	inline constexpr RE::WEAPON_TYPE kDrawnAnimationWeaponType = RE::WEAPON_TYPE::kOneHandMace;

	// Reintentos del paso a maza mientras el modelo no ha llegado a la mano (máx. 1 s).
	inline constexpr std::chrono::milliseconds kDrawnTypePromoteRetryInterval{ 50 };
	inline constexpr int                       kDrawnTypePromoteMaxAttempts = 20;

	// Eventos vanilla de inicio (weaponSwing) y fin (attackStop) del golpe para PowerAttackVFX.
	inline constexpr std::string_view kPowerAttackVfxStartEvent = "weaponSwing";
	inline constexpr std::string_view kPowerAttackVfxStopEvent = "attackStop";

	// Red de seguridad de PowerAttackVFX si no llega attackStop.
	inline constexpr std::chrono::milliseconds kPowerAttackVfxSafetyTimeout{ 3000 };

	// Glow del martillo (GlowMapControl): duración del fundido al encenderse o apagarse.
	inline constexpr float kGlowMapFadeSeconds = 0.5f;

	// Intervalo de búsqueda de criaturas cercanas.
	inline constexpr float kGlowMapCreatureScanIntervalSeconds = 0.5f;

	// Mínimo del pulso como fracción de la intensidad.
	inline constexpr float kGlowMapPulseMinFactor = 0.15f;

	// Keywords vanilla de raza de cada tipo de criatura.
	inline constexpr const char* kGlowMapDragonKeyword = "ActorTypeDragon";
	inline constexpr const char* kGlowMapUndeadKeyword = "ActorTypeUndead";
	inline constexpr const char* kGlowMapDaedraKeyword = "ActorTypeDaedra";

// Red de seguridad: el regreso empieza pasado este margen si no llega la anotación de Call.hkx.
	inline constexpr std::chrono::milliseconds kCallReleaseFallbackWindow{ 1500 };

	// Chasquido de dedos de Llamada (ruta relativa a Data).
	inline constexpr const char* kCallReleaseSoundFilePath = "Sound/FX/ThorMjolnir/ThorMjolnir_FingerSnap.wav";

	// -- Atrape (Open Animation Replacer) --

	// Global que hace que el submod de OAR sustituya el ataque ligero por Catch.hkx.
	inline constexpr const char* kCatchTriggerGlobalEditorID = "CAP_GlobalVariable_ThorMjolnir_CatchTrigger";

// Red de seguridad: el reequipado ocurre pasado este margen si no llega la anotación de Catch.hkx.
	inline constexpr std::chrono::milliseconds kCatchReleaseFallbackWindow{ 1500 };

	// Tras soltar el arma se oculta y el desequipado real espera este margen, para no cortar Throw.hkx.
	inline constexpr std::chrono::milliseconds kThrowReleaseVisualHoldDuration{ 400 };

	// Intervalo de los bucles de Physics con hilos (respaldo sin FrameHook, ~60 por segundo).
	// kTickDeltaSeconds es además el paso de la simulación de la llegada del regreso.
	inline constexpr std::chrono::milliseconds kTickInterval{ 16 };
	inline constexpr float                     kTickDeltaSeconds = 0.016f;

	// Paso máximo de un bucle de Physics por tick, tras un tirón (s).
	inline constexpr float kMaxTickDeltaSeconds = 0.1f;

	// -- Ida (punto 3) --
	// Gravedad del mundo si no se puede leer de Havok (u/s²); velocidad y multiplicador en Settings.
	inline constexpr float kThrowFallbackWorldGravity = -686.614f;  // u/s^2

	// Radio del barrido en cruz de la colisión en vuelo.
	inline constexpr float kThrowCollisionRadius = 25.0f;

	// Retroceso del punto de clavado contra una superficie.
	inline constexpr float kStickEmbedBackoff = 15.0f;

	// Avance del punto de clavado contra un actor (su cápsula es mayor que la malla).
	inline constexpr float kActorStickForwardOffset = 15.0f;

	// Alcance del raycast de la mirilla para calcular la dirección del lanzamiento.
	inline constexpr float kAimRaycastDistance = 6000.0f;

	// -- Regreso (puntos 7-8) --
	// Velocidad al llegar a la mano; la aceleración se calcula por distancia (Return::ComputeReturnAcceleration).
	inline constexpr float kReturnTargetArrivalSpeed = 5000.0f;  // u/s
	// Duración máxima del regreso (s), sin contar el temblor.
	inline constexpr float kReturnMaxDuration = 1.5f;

	// Exponente del perfil de aceleración creciente (2 = aceleración constante).
	inline constexpr float kReturnAccelerationExponent = 2.5f;

	// Tramo final más lento: por debajo de kReturnTailDistance el tiempo avanza hasta kReturnTailMinRate.
	inline constexpr float kReturnTailDistance = 300.0f;
	inline constexpr float kReturnTailMinRate = 0.35f;

	// Distancia a la que el arma ha llegado a la mano.
	inline constexpr float kReturnArrivalDistance = 30.0f;

	// Con Catch.hkx en marcha, el ritmo del vuelo se reescala para llegar a su anotación,
	// sin bajar de Min (no se arrastra) ni pasar de Max (no salta).
	inline constexpr float kReturnRetimeMinRate = 0.5f;
	inline constexpr float kReturnRetimeMaxRate = 2.0f;

	// Tope de la simulación de la llegada prevista (s).
	inline constexpr float kReturnArrivalLookahead = 4.0f;

	// Desvío lateral de la curva: fracción aleatoria de la distancia entre Min y Max, acotada en unidades.
	inline constexpr float kReturnCurveLateralFractionMin = 0.20f;
	inline constexpr float kReturnCurveLateralFractionMax = 0.30f;
	inline constexpr float kReturnCurveMinOffset = 70.0f;
	inline constexpr float kReturnCurveMaxOffset = 350.0f;

	// Posición del punto de control a lo largo de la línea inicio-mano (1/3 desde el inicio).
	inline constexpr float kReturnCurveAnchorFraction = 1.0f / 3.0f;

	// Tiempo que la graph variable "SkipEquipAnimation" se deja activa al reequipar.
	inline constexpr std::chrono::milliseconds kSkipEquipAnimationWindow{ 500 };

	// -- Giro en vuelo (punto 10) --
	// Nodo hijo del NIF del arma que gira; debe coincidir con el nombre en NifSkope.
	inline constexpr std::string_view kWeaponSpinNodeName{ "Mjolnir" };

	// Velocidad angular máxima del giro y eje local (unitario).
	inline constexpr float        kSpinAngularSpeed = 20.0f;  // ~4*pi rad/s
	inline constexpr RE::NiPoint3 kSpinAxisLocal{ 0.0f, 0.0f, 1.0f };

	// Rampa de arranque del giro hasta kSpinAngularSpeed (<= 0 la desactiva).
	inline constexpr float kSpinRampDuration = 0.3f;  // s

	// Antelación con la que empieza el enderezado antes de llegar a la mano.
	inline constexpr float kSpinStraightenLeadTime = 0.2f;  // s

// -- Clavado en un actor (punto 6) --
	// Habilidad de parálisis (Ability, Constant Effect) concedida mientras el arma está clavada.
	// CAP_ThorMjolnir_Ability_ThrowingParalysis, FormID local del ESL.
	inline constexpr RE::FormID kEmbeddedParalysisSpellLocalFormID = 0x019;

	// Efecto de la parálisis, para comprobar si quedó activo (inmunidad).
	// CAP_ThorMjolnir_ParalysisAbilityEffect, FormID local del ESL.
	inline constexpr RE::FormID kEmbeddedParalysisEffectLocalFormID = 0x01A;

	// Hazard eléctrico (con Drop To Ground) colocado sobre el actor al clavarse; FormID local del ESL.
	inline constexpr RE::FormID kEmbeddedHazardLocalFormID = 0x0C3;

	// Hazard eléctrico sin Drop To Ground para impactos contra superficies, orientado por la normal.
	inline constexpr RE::FormID kSurfaceHazardLocalFormID = 0x0C2;

	// Tiempo máximo clavada en un actor; después el arma vuelve sola.
	inline constexpr float kEmbeddedMaxDuration = 5.0f;

	// Margen para confirmar que la parálisis quedó activa; si no, el objetivo es inmune.
	inline constexpr float kImmunityCheckDelay = 0.3f;

	// -- Golpes del regreso (punto 9) --
	// Magnitud del tambaleo (graph variable staggerMagnitude).
	inline constexpr float kStaggerMagnitude = 1.0f;

	// -- Poder Lightning Dash --
	// Lesser Power concedido mientras el arma está equipada.
	// CAP_ThorMjolnir_Spell_LightningDash, FormID local del ESL.
	inline constexpr RE::FormID kLightningDashSpellLocalFormID = 0x00E;

	// Hechizo de cooldown lanzado sobre el jugador al usar el poder y su efecto invisible (10 s).
	// CAP_ThorMjolnir_Spell_LightningDash_Cooldown / _MagicEffect_LightningDash_Cooldown, FormID locales del ESL.
	inline constexpr RE::FormID kLightningDashCooldownSpellLocalFormID = 0x010;
	inline constexpr RE::FormID kLightningDashCooldownEffectLocalFormID = 0x011;

	// Distancia máxima del jugador al arma para desplazarse (~100 m).
	inline constexpr float kLightningDashMaxDistance = 7000.0f;

	// Velocidad del desplazamiento (~100 m/s).
	inline constexpr float kLightningDashSpeed = 7000.0f;  // u/s

	// Separación del destino respecto a la superficie (a lo largo de su normal) y hueco
	// entre el cuerpo del actor clavado y el jugador.
	inline constexpr float kLightningDashSurfaceStandoff = 60.0f;
	inline constexpr float kLightningDashActorGap = 60.0f;

	// Eventos vanilla del grito de sprint, enviados al jugador al empezar, y espera entre ambos.
	inline constexpr const char*               kLightningDashShoutStartEvent = "ShoutStart";
	inline constexpr const char*               kLightningDashSprintStartEvent = "ShoutSprintMediumStart";
	inline constexpr std::chrono::milliseconds kLightningDashSprintEventDelay{ 50 };

	// Avisos en pantalla: arma en la mano, ya volviendo, demasiado lejos y cooldown activo.
	inline constexpr const char* kLightningDashInHandMessage = "Kyne's Thunder must be thrown first.";
	inline constexpr const char* kLightningDashReturningMessage = "Kyne's Thunder is already returning.";
	inline constexpr const char* kLightningDashTooFarMessage = "Kyne's Thunder is too far away.";
	inline constexpr const char* kLightningDashCooldownMessage = "Kyne's Thunder is still recharging.";

	// Tiempo mínimo antes de repetir el mismo aviso (el motor puede comprobar el lanzamiento más de una vez por pulsación).
	inline constexpr float kLightningDashMessageRepeatSeconds = 1.0f;

	// VisualEffect del poder, CAP_ThorMjolnir_VisualEffect_LightningDash (arte LightningStormCastBodyFX + shader
	// CAP_ThorMjolnir_ShockStormFXShader): su arte y su shader van sobre el jugador durante el desplazamiento,
	// y los persistentes que quedaran en una partida se retiran al cargar. FormID local del ESL.
	inline constexpr RE::FormID kLightningDashVisualEffectLocalFormID = 0x013;

	// Modificador de imagen al empezar el desplazamiento (CAP_ThorMjolnir_ImageSPaceMod_LightningDash), FormID local del ESL.
	inline constexpr RE::FormID kLightningDashImageSpaceModLocalFormID = 0x017;

	// Explosiones vanilla sin daño colocadas donde está el jugador al empezar: polvo (FXdustDropSmExplosion)
	// y descarga (ExplosionShockMass01, empuja objetos sueltos).
	inline constexpr RE::FormID       kLightningDashDustExplosionFormID = 0x01A13C;
	inline constexpr RE::FormID       kLightningDashShockExplosionFormID = 0x0D13E8;
	inline constexpr std::string_view kLightningDashVanillaPluginName = "Skyrim.esm";

	// Hueso del esqueleto vanilla donde se ancla la estela del jugador durante el desplazamiento.
	inline constexpr const char* kLightningDashTrailNodeName = "NPC Spine2 [Spn2]";

	// -- Temblor al desclavar (punto 11) --
	// Duración mínima del temblor; BeginReturn puede alargarlo.
	inline constexpr float kStickShudderDuration = 0.5f;

	// Ángulo máximo de la oscilación del temblor.
	inline constexpr float kStickShudderMaxAngle = 0.261799f;  // rad (15°)

	// Fracción del ángulo máximo alcanzada al final del temblor (curva exponencial).
	inline constexpr float kStickShudderAmplitudeRampFraction = 0.95f;

	// Frecuencia del temblor al empezar y al terminar.
	inline constexpr float kStickShudderFrequencyStart = 3.0f;  // Hz
	inline constexpr float kStickShudderFrequencyEnd = 15.0f;   // Hz

	// Eje local (unitario) del temblor.
	inline constexpr RE::NiPoint3 kStickShudderAxisLocal{ 1.0f, 0.0f, 0.0f };

	// Nombre del .esp del mod, para LookupForm con FormID local.
	inline constexpr std::string_view kSoundPluginName = "ThorMjolnirOAR.esp";

	// -- Sonidos (12.- AUDIO) --
	// Silbido del lanzamiento (ruta relativa a Data).
	inline constexpr const char* kThrowLaunchSoundFilePath = "Sound/FX/ThorMjolnir/MjolnirThrow02.wav";

	// Sonidos del atrape: arranque anticipado y golpe final (Audio::CatchCue).
	inline constexpr const char* kCatchStartSoundFilePath = "Sound/FX/ThorMjolnir/MjolnirCall02_Start.wav";
	inline constexpr const char* kCatchEndSoundFilePath = "Sound/FX/ThorMjolnir/MjolnirCall02_End.wav";

	// Prioridad de GetSoundHandleByFile.
	inline constexpr std::uint32_t kFileSoundPriority = 0;

	// Antelación del sonido de arranque respecto a la llegada.
	inline constexpr float kCatchStartSoundLeadTime = 1.066f;

	// Tiempo desde el attackStart de Catch.hkx hasta su anotación de mano cerrada (reloj de FrameHook, medido en el juego).
	// Valor inicial: WeaponManager lo vuelve a medir en cada Atrape y usa la última medida.
	inline constexpr float kCatchAnimationLeadTime = 0.51f;

	// Rango válido de una medida de kCatchAnimationLeadTime (fracción del nominal); fuera de él
	// (tirón durante el gesto, o pausa sin FrameHook) se descarta.
	inline constexpr float kCatchLeadMeasureMinFactor = 0.5f;
	inline constexpr float kCatchLeadMeasureMaxFactor = 2.0f;

	// Medidas recientes de Catch.hkx cuya mediana fija la llegada del Atrape (impar: la mediana es una medida real).
	inline constexpr std::size_t kCatchLeadSampleCount = 5;

	// Red de seguridad: si Catch.hkx no empieza en este margen tras pedirlo, el arma deja de esperarlo.
	inline constexpr std::chrono::milliseconds kCatchStartTimeout{ 1000 };

	// Tiempo mínimo tras la liberación de Call.hkx antes de arrancar Catch.hkx.
	inline constexpr float kMinCatchAnimationDelay = 0.5f;

	// Intervalo mínimo entre dos attackStart por pulsación (Lanzar tras Atrape, Llamada tras Lanzar).
	inline constexpr float kMinAttackStartInterval = 1.0f;

	// -- Cortar un ataque en curso (InterruptAttackThen) --
	// N-ésimo attackStop que marca reposo, espera tras él, red de seguridad y ventana del vigilante.
	inline constexpr int                       kAttackInterruptReadyEventOrdinal = 2;
	inline constexpr std::chrono::milliseconds kAttackInterruptPostEventDelay{ 50 };
	inline constexpr std::chrono::milliseconds kAttackInterruptFallbackDelay{ 450 };
	inline constexpr std::chrono::milliseconds kAttackInterruptWatchWindow{ 1500 };

	// -- Cortar un bloqueo en curso --
	// Evento vanilla que sale del bloqueo sin mezcla y espera antes del gesto.
	inline constexpr const char*               kBlockStopInstantAnimationEvent = "blockStopInstant";
	inline constexpr std::chrono::milliseconds kBlockInterruptSettleDelay{ 50 };

	// Cola de Call.hkx y Catch.hkx tras su anotación antes de enviar attackStop.
	inline constexpr std::chrono::milliseconds kCallAnimationTailDuration{ 250 };
	inline constexpr std::chrono::milliseconds kCatchAnimationTailDuration{ 500 };

	// Espera extra al final del Atrape antes de apagar las chispas (el grafo sigue mezclando).
	inline constexpr std::chrono::milliseconds kCatchVfxSettleDelay{ 400 };

	// Fuerza y duración del temblor de cámara al atrapar (RE::ShakeCamera).
	inline constexpr float kCatchShakeStrength = 20.0f;
	inline constexpr float kCatchShakeDuration = 0.3f;

	// Flags de GetSoundHandleByFile.
	inline constexpr std::uint32_t kSoundHandleFlags = 0x0;

	// Volumen aplicado a cada sonido antes de FadeInPlay.
	inline constexpr float kSoundHandleVolume = 1.0f;

	// Volumen del chasquido de Llamada (más alto: el archivo suena bajo).
	inline constexpr float kCallReleaseSoundVolume = 3.0f;

	// -- Chispas de movimiento (WeaponVFX) --
	// Activator continuo (ThorMjolnirSparks.nif), FormID local del ESL.
	inline constexpr RE::FormID kMovementVfxActivatorLocalFormID = 0x025;

	// Secuencia del NiControllerManager que se activa en los dos .nif de chispas.
	inline constexpr const char* kMovementVfxSequenceName = "partA";

	// Activator "de un solo uso" (ThorMjolnirSparksOff.nif) que apaga las chispas solo.
	inline constexpr RE::FormID kMovementVfxOffActivatorLocalFormID = 0x026;

	// Solape mínimo entre el VFX saliente y el que lo releva antes de destruir el primero.
	inline constexpr std::chrono::milliseconds kMovementVfxSwapOverlapDuration{ 500 };

	// Tiempo máximo antes de destruir el VFX saliente aunque el nuevo no confirme que se ve.
	inline constexpr std::chrono::milliseconds kMovementVfxSwapSafetyTimeout{ 1500 };

	// Vida del "de un solo uso" antes de borrarlo (cubre su ciclo completo).
	inline constexpr std::chrono::milliseconds kMovementVfxFadeOutSafetyMargin{ 2900 };

	inline constexpr float kMovementVfxScale = 1.0f;

	// -- Estela de rayo (WeaponTrail) --
	// NIF de la estela, relativo a meshes/.
	inline constexpr const char* kTrailEffectPath = "Effects/ThorMjolnirTrail.nif";

	// Nodo del NIF de la estela con la cadena de huesos.
	inline constexpr std::string_view kTrailRootNodeName{ "TrailRoot" };

	// Offset del anclaje de la estela en el espacio local del nodo raíz del arma.
	inline constexpr RE::NiPoint3 kTrailAnchorLocalOffset{ 0.0f, 0.0f, 0.0f };

	// Roll de la estela sobre su eje de avance (grados).
	inline constexpr float kTrailRollDegrees = -45.0f;

	// Número de copias de la estela; se reparten en ángulo de forma equitativa.
	inline constexpr std::uint32_t kTrailCopyCount = 4;

	// Separación angular entre copias: 180/N.
	inline constexpr float kTrailCopyRollStepDegrees = 180.0f / static_cast<float>(kTrailCopyCount);

	// Desvío lateral máximo del efecto rayo.
	inline constexpr float kTrailLightningMaxDeviation = 15.0f;

	// Cada cuánto se sortea un desvío nuevo por copia.
	inline constexpr float kTrailLightningHoldSeconds = 0.05f;

	// Longitud de la estela y distancia entre segmentos (reciclado por distancia).
	inline constexpr float kTrailLength = 900.0f;
	inline constexpr float kTrailSegmentSpacing = 30.0f;

	// Escala de cada segmento de la estela.
	inline constexpr float kTrailSegmentScale = 0.5f;

	// -- Destello con luz (WeaponGlow) --

	// NIF del destello, relativo a meshes/.
	inline constexpr const char* kGlowEffectPath = "Effects/ThorMjolnirLight.nif";

	// Nodo de la cabeza del martillo en Mjolnir.nif; el destello sigue su posición.
	inline constexpr std::string_view kWeaponHammerHeadNodeName{ "Gold" };

	// Offset del destello en el espacio local de "Gold".
	inline constexpr RE::NiPoint3 kGlowAnchorLocalOffset{ 0.0f, 15.0f, 0.0f };

	// Activator del destello (kGlowEffectPath), FormID local del ESL.
	inline constexpr RE::FormID kWeaponGlowActivatorLocalFormID = 0x027;

	// Duración del fundido de encendido/apagado del destello (mismas cifras en ms y s).
	inline constexpr std::chrono::milliseconds kGlowFadeDuration{ 300 };
	inline constexpr float                     kGlowFadeDurationSeconds = 0.3f;

	// Velocidad del scroll de "V Offset" del destello, escrito por código cada tick.
	inline constexpr float kGlowUVScrollSpeed = -1.0f / 7.083333f;

	// Malla "RingGlow" de ThorMjolnirLight.nif.
	inline constexpr std::string_view kGlowRingGlowNodeName{ "RingGlow" };

	// Frecuencia y rango del pulso de baseColorScale de "RingGlow".
	inline constexpr float kGlowPulseFrequencyHz = 1.0f;  // ciclos/segundo
	inline constexpr float kGlowPulseScaleMin = 1.2f;
	inline constexpr float kGlowPulseScaleMax = 2.2f;

	// Velocidad y eje local del giro de "RingGlow".
	inline constexpr float        kGlowRingRotationSpeed = 1.5f;  // rad/s
	inline constexpr RE::NiPoint3 kGlowRingRotationAxisLocal{ 0.0f, 0.0f, 1.0f };

	// Luz del destello (TESObjectLIGH), FormID local del ESL.
	inline constexpr RE::FormID kWeaponGlowLightLocalFormID = 0x028;

	// Nombre de nodo reservado para la luz.
	inline constexpr std::string_view kWeaponGlowLightNodeName{ "CAP_ThorMjolnir_GlowLight" };

	// -- Brillo de manos (HandGlow) --
	// Art object (copia de lightningstormhandeffects.nif) aplicado en las manos; FormID local del ESL.
	inline constexpr RE::FormID kHandGlowArtObjectLocalFormID = 0x02A;

	// Duración del brillo de manos; el motor lo retira solo.
	inline constexpr float kHandGlowDuration = 0.6f;

	// Huesos de las manos en el esqueleto vanilla.
	inline constexpr const char* kHandGlowLeftHandNodeName = "NPC L Hand [LHnd]";
	inline constexpr const char* kHandGlowRightHandNodeName = "NPC R Hand [RHnd]";

	// -- Explosión de impacto (WeaponImpactVFX) --

	// BGSExplosion propio colocado en cada impacto de la ida; FormID local del ESL.
	inline constexpr RE::FormID kImpactExplosionLocalFormID = 0x029;
}
