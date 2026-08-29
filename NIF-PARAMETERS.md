# Parámetros controlados por código sobre archivos .nif

Registro de todas las constantes de `src/1.- CORE/Constants.h` que controlan
algo sobre un `.nif` (nombre de nodo buscado por código, FormID de un
Activator/formulario que carga un `.nif`, o una curva/velocidad de animación
escrita por código sobre un campo del NIF) — pensado como catálogo de
partida para un futuro menú MCM que permita editar estos valores sin
recompilar. No incluye constantes sin relación con un `.nif` (temporizadores
de animation graph, sonido, cámara, daño, etc.).

**Regla de mantenimiento (ver también `CLAUDE.md`, sección Workflow): cada
vez que se añada, renombre o elimine una constante de `Constants.h` que
controle algo sobre un `.nif`, actualizar esta tabla en el mismo cambio.**
No es un documento histórico — si un valor deja de coincidir con
`Constants.h`, `Constants.h` manda; corregir aquí.

## Giro en vuelo (punto 10 de `Mecanica del arma.txt`)

.nif afectado: el propio NIF equipable del arma (`Mjolnir.nif`).

| Constante | Tipo | Valor actual | Nodo/campo del NIF | Qué controla |
|---|---|---|---|---|
| `kWeaponSpinNodeName` | `string_view` | `"Mjolnir"` | Nombre del nodo hijo dedicado al giro visual | Nodo sobre el que se escribe `local.rotate` cada tick (`Animation::TickSpin`) |
| `kSpinAngularSpeed` | `float` | `20.0f` (rad/s) | — (no hornea nada, escritura directa) | Velocidad angular del giro en vuelo |
| `kSpinAxisLocal` | `NiPoint3` | `{0,0,1}` | — | Eje local de giro (debe ser unitario, ver el propio comentario en `Constants.h`) |
| `kSpinRampDuration` | `float` | `0.3f` (s) | — | Duración de la rampa de arranque del giro (0→velocidad máxima) |
| `kSpinStraightenLeadTime` | `float` | `0.2f` (s) | — | Antelación del enderezado antes de volver a la mano (`Animation::TickSpinStraighten`) |

## Temblor al clavarse (punto 11)

.nif afectado: el mismo NIF equipable del arma, mismo nodo que el giro
(`kWeaponSpinNodeName`) — el temblor se compone sobre la rotación con la que
el arma se quedó clavada.

| Constante | Tipo | Valor actual | Qué controla |
|---|---|---|---|
| `kStickShudderDuration` | `float` | `0.5f` (s, suelo mínimo) | Duración mínima del temblor antes de desprenderse |
| `kStickShudderMaxAngle` | `float` | `0.261799f` rad (15°) | Amplitud angular máxima de la oscilación |
| `kStickShudderAmplitudeRampFraction` | `float` | `0.95f` | Fracción del máximo alcanzada al final de la envolvente exponencial |
| `kStickShudderFrequencyStart` / `kStickShudderFrequencyEnd` | `float` | `3.0f` / `15.0f` Hz | Frecuencia de la oscilación (chirp de fase continua) |
| `kStickShudderAxisLocal` | `NiPoint3` | `{1,0,0}` | Eje local de la oscilación (distinto del eje de giro a propósito) |

## VFX de movimiento — chispas (`8.- ANIMATION/WeaponVFX.h`)

.nif afectado: `ThorMjolnirSparks.nif` (continuo) / `ThorMjolnirSparksOff.nif`
(de un solo uso, apagado horneado).

| Constante | Tipo | Valor actual | Qué controla |
|---|---|---|---|
| `kMovementVfxActivatorLocalFormID` | `RE::FormID` | `0xB57` | FormID local del Activator del VFX continuo |
| `kMovementVfxOffActivatorLocalFormID` | `RE::FormID` | `0x61C` | FormID local del Activator "de un solo uso" (apagado) |
| `kMovementVfxSequenceName` | `const char*` | `"partA"` | Nombre de la `NiControllerSequence` activada por código en ambos `.nif` |
| `kMovementVfxSwapOverlapDuration` | `chrono::ms` | `500` | Solape mínimo garantizado entre el VFX saliente y el que lo releva |
| `kMovementVfxSwapSafetyTimeout` | `chrono::ms` | `1500` | Red de seguridad absoluta del mismo relevo |
| `kMovementVfxFadeOutSafetyMargin` | `chrono::ms` | `2900` | Margen antes de destruir el "de un solo uso" (debe cubrir su ciclo completo horneado) |
| `kMovementVfxScale` | `float` | `1.0f` | Escala del Activator colocado |

## Estela de rayo (`8.- ANIMATION/WeaponTrail`/`WeaponTrailGroup`)

.nif afectado: `ThorMjolnirTrail.nif` (30 huesos bajo el nodo `TrailRoot`).

| Constante | Tipo | Valor actual | Nodo/campo del NIF | Qué controla |
|---|---|---|---|---|
| `kTrailEffectPath` | `const char*` | `"Effects/ThorMjolnirTrail.nif"` | Ruta del `.nif`, relativa a `meshes/` | Cuál `.nif` se instancia |
| `kTrailRootNodeName` | `string_view` | `"TrailRoot"` | Nombre del nodo padre de la cadena de huesos | Nodo cuyos hijos se reposicionan cada tick |
| `kTrailAnchorLocalOffset` | `NiPoint3` | `{0, 5, 0}` | — | Desplazamiento del punto de anclaje, espacio local del nodo raíz del arma |
| `kTrailRollDegrees` | `float` | `-45.0f` | — | Inclinación fija de la cinta sobre su eje de avance |
| `kTrailCopyCount` | `uint32_t` | `8` | — | Nº de copias en paralelo (`WeaponTrailGroup`) |
| `kTrailCopyRollStepDegrees` | `float` | `22.5f` | — | Separación angular entre copias consecutivas |
| `kTrailLightningMaxDeviation` | `float` | `15.0f` | — | Desviación lateral máxima aleatoria del efecto rayo |
| `kTrailLightningHoldSeconds` | `float` | `0.05f` (s) | — | Cada cuánto se re-sortea el desvío |
| `kTrailLength` | `float` | `900.0f` | — | Alcance total deseado de la estela (asume 30 huesos reales en el NIF) |
| `kTrailSegmentSpacing` | `float` | `30.0f` | — | Distancia entre segmentos consecutivos |
| `kTrailSegmentScale` | `float` | `0.25f` | — | Escala de cada copia individual |

## Destello de la réplica (`8.- ANIMATION/WeaponGlow.h`)

.nif afectado: `ThorMjolnirLight.nif`.

| Constante | Tipo | Valor actual | Nodo/campo del NIF | Qué controla |
|---|---|---|---|---|
| `kGlowEffectPath` | `const char*` | `"Effects/ThorMjolnirLight.nif"` | Ruta del `.nif` | Cuál `.nif` se instancia |
| `kWeaponHammerHeadNodeName` | `string_view` | `"Gold"` | Nombre de nodo real, dentro del NIF **del arma** (`Mjolnir.nif`), no de `ThorMjolnirLight.nif` | Punto que sigue el destello (cabeza del martillo) |
| `kGlowAnchorLocalOffset` | `NiPoint3` | `{0, 15, 0}` | — | Desplazamiento del anclaje en espacio local de `"Gold"` |
| `kWeaponGlowActivatorLocalFormID` | `RE::FormID` | `0xC19` | — | FormID local del Activator del destello |
| `kGlowFadeDuration` / `kGlowFadeDurationSeconds` | `chrono::ms` / `float` | `300` / `0.3f` | — | Duración del fundido de encendido/apagado (malla + luz) |
| `kGlowUVScrollSpeed` | `float` | `-1/7.083333f` | `BSShaderMaterial::texCoordOffset[0]` de la malla bajo el `NiBillboardNode` (sin nombre útil, localizada por estructura) | Velocidad de scroll de UV, derivada de las claves reales horneadas (que nunca se reproducen solas, ver el comentario) |
| `kGlowRingGlowNodeName` | `string_view` | `"RingGlow"` | Nombre real del `BSTriShape` | Malla cuyo `baseColorScale` pulsa |
| `kGlowPulseFrequencyHz` | `float` | `1.0f` | — | Frecuencia del pulso de energía (onda seno) |
| `kGlowPulseScaleMin` / `kGlowPulseScaleMax` | `float` | `0.4f` / `1.6f` | — | Rango de `baseColorScale` del pulso |
| `kWeaponGlowLightLocalFormID` | `RE::FormID` | `0x6DE` | — | FormID local del `TESObjectLIGH` (luz dinámica, pendiente de conectar del todo) |
| `kWeaponGlowLightNodeName` | `string_view` | `"CAP_ThorMjolnir_GlowLight"` | — | Nombre que tendrá el `NiPointLight` creado por código |

## VFX de impacto (`8.- ANIMATION/WeaponImpactVFX.h`) -- histórico 2026-08-28/29, ya no aplica

**Sección retirada (2026-08-29): el VFX de impacto ya no usa ningún `.nif`
propio**, así que queda fuera del alcance de este documento (registro de
constantes que controlan algo sobre un `.nif`). Tras varias rondas con un
`ThorMjolnirImpact.nif` propio (copiado de `fxshockcloakhandeffects.nif`,
con crashes de carga nativa nunca pinpointeados del todo pese a
verificación exhaustiva -- ver `CHANGELOG.md` v1.17.1-v1.17.9 para el
histórico completo), el usuario pasó a usar una explosión vanilla
(`ExplosionRuneShock01`) como plantilla y duplicó un `BGSExplosion` propio
en la Creation Kit (`CAP_ThorMjolnir_Explosion...`, FormID `0x0101BC69` en
`ThorMjolnirOAR.esp`, enmascarado a 12 bits por ESL --
`Constants::kImpactExplosionLocalFormID = 0xC69`), colocado con
`RE::TESObjectREFR::PlaceObjectAtMe` -- sin `.nif`, sin NifSkope, sin nada
que animar por código; el motor lo reproduce y limpia solo. El tamaño
visual se ajusta con el campo `Radius` de la propia explosión en la
Creation Kit (`BGSExplosionData::radius`), no con ningún parámetro de
código. `kImpactExplosionLocalFormID` es un FormID de plugin, no un
parámetro de `.nif`, así que no le corresponde fila en esta tabla.

## Notas para un futuro menú MCM

- Todas las constantes son `inline constexpr` en `Constants.h` — un menú MCM
  real necesitaría convertir el subconjunto editable a variables normales
  leídas de un INI/SKSE co-save, no a constantes de compilación. Este
  documento cataloga *qué* sería editable, no implementa el mecanismo de
  edición en sí.
- Las filas con "Nodo/campo del NIF" marcadas `—` son curvas/velocidades
  puramente numéricas (sin nombre de nodo que resolver) — más directas de
  exponer en un menú que las que sí dependen de que un nombre de nodo siga
  existiendo tal cual en el `.nif` real.
- Los FormID (`RE::FormID`) son locales, ya enmascarados a 12 bits por el
  flag ESL de `ThorMjolnirOAR.esp` (ver `CLAUDE.md`, "Errores comunes a
  vigilar") — no editables con sentido desde un menú (identifican qué
  Activator/formulario cargar, no un valor de ajuste), se listan aquí solo
  como referencia de qué `.nif` corresponde a cada FormID.
