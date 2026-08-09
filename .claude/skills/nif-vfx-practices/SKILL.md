---
name: nif-vfx-practices
description: Ayuda a configurar/editar archivos .nif de Skyrim (NifSkope) sin inventar nombres de bloque, flags, sockets ni pasos, especialmente para VFX (brillo/emisivo, partículas, shaders de efecto, estelas, animaciones horneadas en el NIF). Úsala siempre que el usuario pida crear, modificar o depurar algo en un .nif, en NifSkope, o un efecto visual de un arma/objeto (glow, burning, trail, partículas, spin horneado), incluso si no lo pide con la palabra "skill".
---

# Configurar NIFs y VFX sin inventar

Un `.nif` es un formato binario versionado (Gamebryo/NetImmerse) con cientos de
tipos de bloque, cuyos nombres, flags y orden de slots cambian entre versión
de formato (LE usa `NiTriShape`, SSE usa `BSTriShape`, por ejemplo) y entre
shader types. La memoria de entrenamiento sobre NIFs mezcla tutoriales de
distintas épocas/juegos (Oblivion, FO3/NV, Skyrim LE, Skyrim SE) y es una
fuente de errores silenciosos: un nombre de bloque o un índice de slot que
"suena bien" pero no es el real no da error de compilación — simplemente no
funciona en el juego, o peor, corrompe el NIF.

**Regla rectora: para cualquier afirmación concreta sobre estructura de un
NIF (nombre de bloque, propiedad, flag, orden de texture slot, nombre de
nodo/hueso, técnica de VFX), cita de dónde sale.** Si no puedes citarla según
la jerarquía de abajo, dilo explícitamente ("no verificado, hay que
comprobarlo en NifSkope/en el juego") en vez de darla por buena. Mismo
criterio que ya exige `CLAUDE.md` del proyecto para el documento de diseño y
para las APIs de CommonLibSSE-NG.

## Jerarquía de fuentes de verdad (en este orden)

1. **Ya verificado en este proyecto** — `CLAUDE.md`, sección "Animación
   horneada en el NIF (punto 10, giro)": patrón confirmado *en el juego* de
   un `NiTransformController` colgado de un nodo hijo dedicado
   (`Constants::kWeaponSpinNodeName`), flag `kActive` desactivado en NifSkope
   para que no arranque solo, `Start()`/`Stop()` virtuales sin
   `NiControllerManager`/`NiControllerSequence` de por medio, y la trampa de
   reenganchar *todas* las mallas visibles al nuevo nodo. Si la pregunta cae
   dentro de esto, usa ese texto tal cual — no lo reinventes ni lo
   "mejores" sin que el usuario lo pida.
2. **NIFs reales ya funcionando, disponibles en este repo**:
   - `_reference/Kratos Combat - 2.8.6a/meshes/`: mod publicado, y además la
     plantilla que este proyecto está obligado a seguir para todo lo
     relacionado con el arma arrojadiza/de retorno (ver memoria
     `feedback-seguir-metodologia-kratos` — no simplificar por libre,
     replicar su patrón). No contiene partículas ni UVs animados — solo
     sirve para el giro horneado y el shader de brillo/emisivo.
   - `_reference/Nif examples/meshes/`: mezcla de varias fuentes reales,
     cada subcarpeta con su propia procedencia:
     - `stormcalling/`, `stormcalling_ii/`: mods de magia de rayos
       publicados ("Storm Calling"/"Storm Calling II", Nexus) — fuente
       para **partículas y controladores de shader** en general.
     - `Effects/WeaponTrails/` (`AttackTrail.nif`,
       `AttackTrailMagic.nif`): del mod **Precision** (Ershin) — el mod de
       detección de golpes con estela de arma más usado del ecosistema de
       Skyrim SE. **Es la referencia más directamente relevante para una
       futura estela del hacha en vuelo/swing** — ver el patrón dedicado
       más abajo, "Estela de arma".
     - `magic/lightspellprojectile.nif`, `weapons/orcish/orcisharrowprojectile.nif`,
       `meshes/dlc01/weapons/crossbow/boltprojectile.nif`,
       `magic/lightningstormhandeffects.nif`, `magic/shockhandeffects.nif`:
       mallas **vanilla** de Skyrim (proyectil de hechizo de luz, flecha
       órcica, virote de ballesta DLC, efectos de mano de tormenta/shock) —
       la fuente más autorizada posible para "cómo lo hace Bethesda", con
       la salvedad de que son de una época distinta (formato/estilo
       pre-SSE en textura de nodos) y pueden no reflejar las convenciones
       más nuevas de `BSTriShape`/SSE al 100%.
     
     Igual que con Kratos, no se ha confirmado que *este usuario* haya
     probado estos ficheros en el juego dentro de este proyecto (a
     diferencia del patrón de giro de `CLAUDE.md`, que sí está verificado
     en el juego) — trátalos como "mod publicado real"/"asset vanilla del
     juego", un escalón más fiable que la especulación pero no al nivel de
     "verificado en el juego por el usuario de este repo".
   
   Antes de describir cómo se construye un efecto, **inspecciona el NIF
   real** en vez de recordarlo de memoria — ver más abajo cómo, con los
   métodos ya probados en este entorno concreto. Si el usuario trae un NIF
   propio (de otro mod, o un asset nuevo), es una fuente igual de válida —
   inspecciónalo igual antes de opinar sobre su estructura.
3. **Documentación oficial/comunidad reconocida, citada con URL** — NifTools
   (el propio `nifxml` en `github.com/niftools/nifxml` es la fuente que usa
   NifSkope para sus definiciones de bloque — más fiable que cualquier
   tutorial), Creation Kit wiki (`creationkit.com`/`ck.uesp.net`), o guías de
   modding de autores reconocidos. Usa `WebSearch`/`WebFetch` para esto y
   cita la URL exacta en la respuesta — no la memoria de entrenamiento sobre
   "cómo se hace esto en Skyrim" sin más. Ya hay un extracto verbatim
   descargado y citado en `references/nifxml-excerpts.md` (UVs en
   movimiento y partículas, ver más abajo) — consúltalo antes de volver a
   descargar `nif.xml` (pesa ~570 KB / 8700 líneas; para bloques que no
   estén ya ahí, sí vuelve a descargarlo: `curl -sL -o nif.xml
   https://raw.githubusercontent.com/niftools/nifxml/develop/nif.xml` y
   `grep`/`sed` sobre el fichero local, `WebFetch` directo sobre esa URL
   trunca el contenido).
4. **Nunca** des por buena de memoria de entrenamiento una lista de flags, un
   valor numérico de flag, un orden de texture slot, o un nombre de bloque
   exacto sin haber pasado por 1-3. Si ninguna de las tres aplica, dilo.

**Matiz importante entre fuentes 2 y 3**: un NIF real inspeccionado (fuente
2) confirma que algo *funciona de verdad en el juego* — es el mod publicado
tal cual. `nif.xml` (fuente 3) solo confirma que un bloque/campo *existe en
el formato* y cómo se llama — no que produzca el resultado visual esperado
ni que sea la técnica que usa Skyrim realmente (puede haber varias formas
válidas de lograr lo mismo, o el campo puede estar deprecado/sin usar en la
práctica). Cuando una respuesta se apoye solo en fuente 3, dilo así
explícitamente — "confirmado contra el formato, no contra un NIF real
funcionando" — y recomienda probarlo en NifSkope/en el juego antes de darlo
por bueno.

## Cómo inspeccionar un NIF real (métodos ya probados en este PC)

### a) Grep binario de nombres de tipo de bloque (siempre funciona)

El nombre de cada tipo de bloque usado en un NIF aparece como string ASCII
literal en el fichero, sea cual sea la versión de formato. Es el método más
robusto para un reconocimiento rápido — úsalo primero:

```python
import re
data = open(path, 'rb').read()
strings = [s.decode('ascii', errors='ignore') for s in re.findall(rb'[ -~]{4,}', data)]
# tipos de bloque: strings que empiezan por Ni/BS/bhk y no llevan espacios
blocktypes = [t for t in strings if t[:1].isupper() and ('Ni' in t or 'BS' in t or 'bhk' in t) and ' ' not in t and len(t) < 40]
# rutas de textura
textures = [t for t in strings if t.lower().endswith('.dds')]
```

También sirve `grep -rl "NiParticleSystem\|BSEffectShaderProperty\|BSStripParticleSystem" --include="*.nif" <carpeta>`
para localizar rápido qué NIFs de una carpeta usan qué técnica, antes de
abrir nada en NifSkope.

### b) PyFFI — instalado en este PC, pero con límites reales, verificados

`pip show pyffi` confirma PyFFI 2.2.3 instalado. Dos problemas reales
comprobados en este entorno, no supuestos:

- Con Python 3.10+ falla al importar (`AttributeError: module 'time' has no
  attribute 'clock'`, porque `time.clock()` se eliminó en 3.8). Se evita con
  un monkeypatch antes de importar: `time.clock = time.perf_counter`.
- Incluso con el parche, **no reconoce `BSTriShape`** (el tipo de malla
  nativo de Skyrim SE) — falla con `ValueError: Unknown block type
  'BSTriShape'` al intentar leer cualquier NIF SSE moderno con mallas de
  personaje/arma típicas. Solo lee sin problema NIFs de estilo más antiguo
  basados en `NiTriShape` (comprobado: los `Projectile_LeviathanAxe*.nif` de
  Kratos sí se leen bien, `Blade_of_Chaos*.nif` con `BSTriShape` no — mismo
  fallo reproducido también en todos los NIFs de partículas de
  `_reference/Nif examples/`, que sí llevan `BSTriShape` en alguna parte de
  la geometría aunque el efecto en sí sea partículas).

Conclusión práctica: usa PyFFI (con el parche) solo para inspeccionar
jerarquía de nodos/controladores en NIFs de estilo legado (`NiTriShape`). Si
falla con "Unknown block type", no es un bug tuyo — cae de vuelta al método
(a), o al método (c) de abajo si lo que hace falta es el valor de un campo
concreto de tamaño fijo, o pide al usuario que abra NifSkope y reporte el
árbol de bloques o haga una captura de pantalla. En particular, **el valor
exacto de un campo concreto** (p. ej. qué entero lleva `Controlled
Variable` en una instancia real de `BSEffectShaderPropertyFloatController`
— si de verdad anima U/V Offset o más bien Emissive/Alpha) **no** se puede
sacar del método (a) — el grep binario solo confirma qué *tipos de bloque*
coexisten, no los valores de sus campos. Para eso, ver el método (c).

```python
import time
if not hasattr(time, 'clock'):
    time.clock = time.perf_counter
from pyffi.formats.nif import NifFormat
data = NifFormat.Data()
with open(path, 'rb') as f:
    data.read(f)
def walk(block, depth=0, seen=None):
    seen = seen or set()
    if id(block) in seen: return
    seen.add(id(block))
    print('  ' * depth + type(block).__name__)
    for link in block.get_refs():
        walk(link, depth + 1, seen)
for r in data.roots:
    walk(r)
```

### c) Lector binario dirigido — lee valores numéricos reales, incluso en ficheros SSE que PyFFI no puede abrir

El header de un NIF (`nif.xml`, `struct Header`, campo `Block Size`,
`since="20.2.0.5"` — Skyrim SE es 20.2.0.7, así que aplica) incluye una
**tabla con el tamaño en bytes de cada bloque**. Eso permite localizar los
bytes exactos de cualquier instancia de un tipo de bloque sin necesidad de
entender su estructura interna completa (el problema que hace fallar a
PyFFI con `BSTriShape`) — solo hace falta conocer el layout fijo del
*campo concreto* que interesa, tomado de `nif.xml`.

Ya implementado y **validado** (2026-08-07):
`scripts/nif_header_walk.py` (parsea el header: tipos de bloque, índice de
tipo por bloque, tabla de tamaños) + `scripts/nif_find_controlled_variable.py`
(ejemplo concreto: localiza todas las instancias de
`BSEffectShaderPropertyFloatController`/`BSLightingShaderPropertyFloatController`
y decodifica su campo `Controlled Variable`, los últimos 4 bytes del
bloque — el resto del layout, `NiTimeController`(26B) +
`NiSingleInterpController.Interpolator`(4B), sale de los campos ya
extraídos en `references/nifxml-excerpts.md`). Validación: se contrastó
`nif_header_walk.py` contra
`Projectile_LeviathanAxeA.nif` de Kratos (el único NIF local que PyFFI lee
entero) y el árbol de bloques coincidió bloque a bloque, mismo orden, con
el que ya había dado PyFFI antes — y el final del último bloque más el
footer cuadró exactamente con el tamaño real del fichero.

Uso (probado desde su ubicación real, `scripts/` dentro de esta skill):
`cd` a `scripts/` y `python nif_find_controlled_variable.py ruta1.nif
ruta2.nif ...` (requiere que `nif_header_walk.py` esté en el mismo
directorio o en `PYTHONPATH`, porque lo importa). Si vas a decodificar un campo de un tipo de bloque
*distinto*, no reutilices los offsets tal cual — recalcula el tamaño fijo
esperado desde `nif.xml` (mismo proceso que en "Cómo inspeccionar") y
compruébalo contra el tamaño real leído del fichero antes de fiarte del
valor decodificado (el script ya avisa si el tamaño no es el esperado).
No sirve para bloques de tamaño variable (con arrays internos, listas de
texturas, etc.) sin adaptarlo.

**Resultados reales ya obtenidos con esto** (ver la sección de patrones
más abajo, "UVs en movimiento"): confirma qué anima cada
`BSEffectShaderPropertyFloatController`/`BSLightingShaderPropertyFloatController`
real en `AttackTrail(Magic).nif` (Precision), `stafflightningproj.nif`
(Storm Calling) y `lightspellprojectile.nif` (vanilla) — ya no es
necesario abrir NifSkope para saber esto en estos ficheros concretos.

### d) NifSkope manual

Sigue siendo la vía principal para *editar* de verdad, y para confirmar
cualquier campo que el método (c) no cubra (bloques de tamaño variable, o
verificación visual). Instalado en este PC en:
`C:\Users\bbarc\Desktop\SKYRIM MODDING\2.- HERRAMIENTAS\NifSkope\NifSkope.exe`.
Comprobado (2026-08-07): esta build **no tiene modo de línea de
comandos/scripting** (`--help` no da salida ni opciones; el `README.txt`
solo documenta uso gráfico) — es GUI-only, esta skill no puede pilotarla.

Ojo: ese NifSkope lleva su propio `nif.xml` embebido en la misma carpeta,
fechado en **2018** (7339 líneas) — más antiguo y menos completo que la
copia de `github.com/niftools/nifxml` rama `develop` ya extractada en
`references/nifxml-excerpts.md` (8667 líneas, descargada 2026-08-07). Si
un campo documentado aquí no aparece en el Block Details de este NifSkope
concreto, no asumas que el campo no existe en el formato — puede ser solo
que esta build no lo conoce todavía; contrástalo contra el extracto local
antes de descartarlo.

### e) Scripts adicionales, validados 2026-08-09 (sesión de `fxsparkfountaintoggle.nif`/spark fountain)

- `scripts/nif_sequence_decode.py`: decodifica `NiControllerManager` (secuencias
  que referencia, `Object Palette`) y cada `NiControllerSequence` (`Cycle
  Type`/`Frequency`/`Start Time`/`Stop Time` + su lista `Controlled Blocks`
  completa, con el `Interpolator`/`Controller` de cada entrada y sus nombres
  reales vía tabla de strings — `Node Name`/`Controller Type`/`Interpolator
  ID`). Es la herramienta correcta para "¿qué anima esta secuencia y con qué
  interpolador?" sin tener que teclear los offsets a mano cada vez. Uso:
  `python nif_sequence_decode.py fichero.nif`.
- `scripts/nif_keydata_decode.py`: decodifica un `NiFloatData` (`KeyGroup<float>`)
  entero — número de claves, tipo de interpolación (`LINEAR_KEY`/`QUADRATIC_KEY`)
  y cada clave con `Time`/`Value` (+ `Forward`/`Backward` si es
  `QUADRATIC_KEY`). Uso: `python nif_keydata_decode.py fichero.nif
  <índice_bloque> [índice_bloque...]`.
- `scripts/nif_psysdata_maxparticles.py`: decodifica `NiPSysData`, en
  particular **`BS Max Vertices`** (ver más abajo, "El límite de partículas
  simultáneas") — específico de la versión 20.2.0.7/`bs_version`≥100
  (macro `#BS202#` de `nif.xml`). Uso: `python nif_psysdata_maxparticles.py
  fichero.nif`.

Los tres siguen el mismo patrón que los anteriores (importan
`nif_header_walk`, hay que ejecutarlos desde `scripts/` o con esa carpeta en
`PYTHONPATH`) y están validados contra un fichero real de este proyecto
(comprobado que el tamaño de bloque calculado cuadra con el real, o — para
`NiPSysData`, donde varios campos finales no aplican en BS202 y el cuadre no
es exacto — que los datos que sí caen dentro de la parte calculada tienen
sentido real, ver el comentario del propio script).

### Sobre editar un NIF por script (no solo leer)

Técnicamente es posible escribir bytes en un `.nif` con Python (PyFFI
tiene su propio `.write()`, no probado; o escritura binaria dirigida igual
que en (c)) — pero no se ha intentado ni validado, y el riesgo es mucho
mayor que al leer: una edición real tiene que mantener consistentes la
tabla de tamaños de bloque, la tabla de strings y los índices de
referencia (`Ref`/`Ptr`) entre bloques a la vez; un solo desajuste
corrompe el fichero. A diferencia de leer, tampoco hay forma de comprobar
el resultado visualmente (no hay captura de la vista 3D de NifSkope, no se
puede abrir el juego desde aquí). **Recomendación: la edición real la
sigue haciendo el usuario en NifSkope** — esta skill indica exactamente
qué bloque/campo tocar y qué valor poner, y verifica el resultado
releyendo el fichero con los métodos (a)/(c) después.

## Patrones de VFX ya observados (con procedencia exacta — no extrapoles más allá de lo citado)

- **Estela de arma (weapon trail) — el patrón más relevante para el mecanismo
  de este proyecto.** Dos variantes reales, ambas del mod **Precision**
  (`_reference/Nif examples/meshes/Effects/WeaponTrails/`):
  - `AttackTrail.nif` (la base): **no usa ningún sistema de partículas**.
    Es una malla en forma de cinta/estela **rigged a huesos**
    (`NiSkinData`/`NiSkinInstance`/`NiSkinPartition` + una cadena de nodos
    `Bone001`...`Bone011`), con nodos llamados literalmente `Trail`,
    `TrailRoot` y `Refract`, bajo un `BSFadeNode` raíz ("Scene Root").
    Shaders: `BSLightingShaderProperty` (base) + `BSEffectShaderProperty`
    (el brillo/blend aditivo) + `NiAlphaProperty`, más un
    `BSLightingShaderPropertyFloatController` animando un float del
    shader base — **confirmado por decodificación real** (método (c),
    2026-08-07, no ya solo hipótesis por el nombre del nodo): el
    `Controlled Variable` de ese controlador vale `0` = **Refraction
    Strength**, tanto en `AttackTrail.nif` como en `AttackTrailMagic.nif`.
    El trail de Precision anima **distorsión/refracción**, no un scroll de
    UV ni un fade de alpha — coherente con el nombre del nodo (`Refract`).
    La forma de la estela en sí es geometría animada por huesos, no
    partículas — en Precision, el propio plugin SKSE reposiciona esos
    huesos cada fotograma para que la estela siga el arco real del
    swing del arma (comportamiento de la lógica del mod, no algo que esté
    "dentro" del NIF — el NIF solo aporta la malla/shader, el movimiento
    es código, igual de filosofía que el control manual por tick que ya
    usa este proyecto para la ida/vuelta del hacha).
  - `AttackTrailMagic.nif` (variante "mágica"): mismo esqueleto/malla base
    que arriba, más una capa de `NiParticleSystem` con `NiPSysMeshEmitter`
    (emite justo desde la malla de la estela) y modificadores
    `NiPSysAgeDeathModifier`/`BoundUpdateModifier`/`GravityModifier`/
    `PositionModifier`/`RotationModifier`/`SpawnModifier` +
    `BSPSysScaleModifier`/`SimpleColorModifier`/`LODModifier`, y un nodo
    extra `Trail Fade`. Confirma en un mod real la combinación "malla
    rigged + partículas emitidas desde esa malla" para un efecto más
    vistoso/"encantado" — relevante si el hacha debe verse con algo de
    magia además de la estela base.
  - **Alternativa mucho más simple, confirmada en assets vanilla**:
    `weapons/orcish/orcisharrowprojectile.nif` y
    `meshes/dlc01/weapons/crossbow/boltprojectile.nif` (flecha/virote
    reales del juego base) llevan, colgado de un nodo `TracerRoot`, un
    `BSTriShape` extra nombrado `trailShort` con su propio
    `BSEffectShaderProperty` — **sin partículas, sin rigging a huesos**,
    solo una geometría pequeña adicional con shader de efecto. Es la
    estela más barata posible, confirmada en el propio juego. Buen punto
    de partida antes de intentar replicar algo tan elaborado como
    Precision.
  - **Otra alternativa vanilla, distinta técnica**: `magic/lightspellprojectile.nif`
    usa un `NiBillboardNode` (nodo que siempre mira a cámara — bloque
    documentado en `references/nifxml-excerpts.md`) envolviendo varios
    `NiPSysBoxEmitter` con nombres de sub-objeto típicos de un pipeline de
    3ds Max (`Wisps04`, `Flash03`, `SuperSpray07-Emitter`) — partículas
    tipo sprite/billboard en vez de emisor-por-malla. Confirma que el
    estilo "billboard + caja emisora" también es un patrón vanilla real,
    distinto del emisor-por-malla de Precision/Storm Calling.
- **Luz dinámica de verdad (el "glow que irradia luz", no solo el shader)** —
  `magic/lightspellprojectile.nif` (vanilla) tiene un nodo llamado
  `AttachLight02` (confirmado por grep binario). No es un bloque `NiLight`
  horneado en el NIF (ninguno de los NIFs de referencia de este repo lleva
  `NiLight`/`NiPointLight`) — es el punto donde el motor engancha en
  tiempo de ejecución un form **Light** (`TESObjectLIGH`) real, con su
  propio radio/color/parpadeo configurados en la Creation Kit, no en el
  NIF. Confirmado como convención real (no solo por este fichero) vía
  WebSearch — mods de luz de hechizo tipo Candlelight documentan el mismo
  nodo `AttachLight` para enganchar la luz de la mano. **No confirmado con
  precisión**: la regla exacta de qué campo del Magic Effect/formulario
  enlaza el nombre del nodo con el form Light concreto, ni si hace falta
  el sufijo numérico (`02`) o vale `AttachLight` a secas — verificarlo en
  la Creation Kit/CK wiki si hace falta implementarlo. Para una estela de
  arma esto sería una capa aparte del brillo del shader
  (`BSEffectShaderProperty`/emissive, ya documentado más abajo) — el
  `BSEffectShaderProperty` da el brillo de la propia geometría, un `Light`
  real en `AttachLight` iluminaría el entorno alrededor (paredes, el
  propio jugador) además.
- **Composición modular vía `BSValueNode`/AddOnNode** — visto en
  `orcisharrowprojectile.nif`/`boltprojectile.nif` (vanilla): el mecanismo
  nativo de Bethesda para montar un efecto a partir de varias piezas NIF
  reutilizables (chispas/humo/luz cada una en su propio fichero, referenciadas
  por ID) en vez de hornearlo todo junto — ver `references/nifxml-excerpts.md`
  para el detalle. Alternativa real al patrón de "todo en un nodo hijo del
  NIF del arma" que ya usa este proyecto para el giro; probablemente no
  compensa el coste de introducir un tipo de form nuevo solo para la
  estela, pero es la referencia de "cómo lo hace Bethesda de verdad" si el
  efecto crece mucho en piezas.
- **Efectos de mano ("handeffect") — arquitectura multi-fase, distinta de
  todo lo anterior.** Fuente:
  `_reference/Nif examples/meshes/magic/lightningstormhandeffects.nif` y
  `shockhandeffects.nif` (vanilla, los efectos de mano reales de los
  hechizos de tormenta/shock). Mecanismo CK (confirmado vía WebSearch,
  `ck.uesp.net/wiki/MagicEffect_Script` — la página del Magic Effect en sí
  bloqueó el fetch directo, cita basada en el resumen de búsqueda): el
  registro **Magic Effect** de la Creation Kit tiene un campo de "arte de
  fundido"/casting art — el `MagicEffect Script` expone funciones para
  leer/poner "el arte que se muestra en las manos mientras se lanza el
  hechizo" — ese NIF es el que se engancha ahí, no algo que decida el
  propio NIF por sí solo.
  
  Estructura real de estos dos ficheros (inspeccionados 2026-08-07,
  métodos (a) y (c)):
  - `BSFadeNode` raíz, con `NiControllerManager` +
    `NiMultiTargetTransformController` y **varias `NiControllerSequence`
    con nombres que son fases reales de lanzar un hechizo**: `mIntro`,
    `mIdle`, `mCharge`, `mReadyTrans` (solo tormenta), `mReady`, `mCast`,
    `mCastCon` (solo shock, "cast continuo" — hechizos de concentración),
    `mIdleStaff`. No es una animación suelta, es un set de fases
    encadenadas.
  - `BSBehaviorGraphExtraData` ("BGED") con `Behaviour Graph File` =
    `Magic\CastingRitualBody.hkx` (tormenta) / `Magic\CastingWithIntro.hkx`
    (shock) — confirmado en `nif.xml`: este bloque "Links a nif with a
    Havok Behavior .hkx animation file". Es el mismo tipo de bloque que ya
    lleva el propio NIF del hacha de Kratos (`BGED`, visto en la primera
    inspección de este proyecto) — ahora con su función exacta confirmada:
    sincroniza qué `NiControllerSequence` se reproduce con la máquina de
    estados de animación real del hechizo.
  - Composición en capas, mismos nombres reveladores que en Storm
    Calling/Precision: una malla de brillo central (`BrightGlowMesh`,
    `pFireballCore04`) + emisores de partículas con sufijo `-Emitter`
    (`Sparks02-Emitter`, `pFireballCore04-Emitter`) — núcleo brillante +
    chispas alrededor, no una única pieza.
  - **Valores reales decodificados (método (c))** — contraste útil con la
    estela de rayos: aquí **no hay scroll de UV**, los
    `BSEffectShaderPropertyFloatController` animan `Alpha Transparency`
    (los dos ficheros) y `EmissiveMultiple` (solo shock, pulso de brillo).
    El "fluir" de un handeffect se consigue con partículas + fundido de
    alpha, no con textura que se desplaza — técnica distinta a la del
    trail de rayo, aunque el bloque sea el mismo tipo.
  - `AttachLight` reaparece en `shockhandeffects.nif`, esta vez **sin**
    sufijo numérico (`AttachLight` a secas, no `AttachLight02` como en
    `lightspellprojectile.nif`) — el sufijo no es obligatorio.
  - Tres bloques nuevos, ya extraídos de `nif.xml`
    (`references/nifxml-excerpts.md`): `BSPSysInheritVelocityModifier`
    (las partículas heredan velocidad de otro objeto — relevante si las
    chispas del hacha en vuelo deben arrastrar algo de su propia
    velocidad en vez de quedarse flotando donde nacieron),
    `NiPSysEmitterInitialRadiusCtlr`/`NiPSysEmitterLifeSpanCtlr` (animan
    el tamaño/duración del emisor con el tiempo — el efecto de "carga"
    creciendo mientras se mantiene pulsado el hechizo).
  - Nodo `BillboardHelperHACK` visto en `lightningstormhandeffects.nif` —
    nombre real tal cual, función exacta no investigada, no asumir para
    qué es.
  
  **Importante para este proyecto**: esto es la arquitectura real detrás
  de un efecto "que se carga y luego se dispara" (Intro→Charge→Ready→Cast),
  no solo un VFX estático — si lo que se busca es replicar *esa técnica*
  (fases encadenadas, activadas por código en el momento justo) aplicada al
  hacha, encaja con el patrón `Start()`/`Stop()` ya verificado del giro,
  extendido a varias fases en vez de una sola. Si en cambio se busca un
  efecto literalmente enganchado a la mano del jugador (no al arma), hace
  falta un form Magic Effect/Spell nuevo — eso ya no es este skill de NIFs,
  es diseño de formularios de la Creation Kit **+ API de
  CommonLibSSE-NG (dominio de `verify-commonlibsse-api`, no de esta
  skill)**.

  **Mecanismo C++ real del "efecto literal en la mano", verificado
  2026-08-07 contra `lib/commonlibsse-ng/include` (no de memoria):**
  - `RE::EffectSetting::Data` tiene `castingArt`/`hitEffectArt` (dos
    `BGSArtObject*` distintos, offsets `0x80`/`0x88`,
    `EffectSetting.h:95-96`) — confirma que es el registro **Magic
    Effect** el que enlaza el NIF con el hechizo, como ya decía la fuente
    3 (WebSearch), ahora confirmado contra el header real.
  - `RE::ActorMagicCaster` (`ActorMagicCaster.h`) es el "lanzador" real
    por mano de un actor: tiene `MagicSystem::CastingSource
    castingSource` (`kLeftHand`/`kRightHand`/`kOther`/`kInstant`), un
    `BGSArtObject* castingArt` propio, y **virtuales que son exactamente
    las fases del NIF**: `StartChargeImpl()`, `StartReadyImpl()`,
    `StartCastImpl()`, `FinishCastImpl()`, `InterruptCastImpl()` — mismo
    nombre de concepto que `mCharge`/`mReady`/`mCast` en las
    `NiControllerSequence` de los NIFs de handeffect. Confirma que el
    diseño del NIF y la máquina de estados de C++ están pensados juntos,
    no son cosas separadas.
  - `ActorMagicCaster::light` (`NiPointer<BSLight>`, offset `0xC8`) — una
    luz dinámica real gestionada por el propio caster. Confirma con más
    fuerza el hallazgo de `AttachLight`: el "glow que irradia luz" de un
    efecto de mano no es solo shader, es literalmente un `BSLight` que el
    motor crea y adjunta.
  - **No verificado todavía**: la función pública para *disparar* un cast
    desde fuera (algo tipo `Actor::CastSpellImmediate`, sin confirmar que
    exista con ese nombre/firma) ni cómo obtener el `ActorMagicCaster` de
    una mano concreta desde un `Actor*`. Eso hace falta investigarlo con
    `verify-commonlibsse-api` cuando se vaya a implementar de verdad —
    esta skill llega hasta "cómo se construye el NIF y por qué encaja con
    esta máquina de estados", no hasta el código C++ que lo dispara.

  **Cómo "sabe" el NIF que va a la mano y qué lo envuelve — respuesta
  corta: no lo sabe, y no lo envuelve (no hay rigging).** Verificado
  decodificando el bloque raíz y los emisores de partículas de los dos
  ficheros (método (c), 2026-08-07):
  - **Sin skinning en absoluto**: ninguno de los dos lleva
    `NiSkinData`/`NiSkinInstance`/`NiSkinPartition` (comprobado por grep
    binario — cero coincidencias de "Skin" en ambos ficheros). No hay
    ningún hueso de la mano/dedos referenciado dentro del NIF. El efecto
    no se deforma con la pose de la mano, es geometría/partículas rígidas.
  - **El nodo raíz (`BSFadeNode`) está en el origen local**: `Translation
    = (0, 0, 0)`, `Scale = 1.0` en los dos ficheros (y, para contraste, en
    el propio NIF del hacha de Kratos también) — sin ningún offset
    horneado. Esto es la clave real: el NIF no necesita "saber" dónde
    está la mano porque no lleva ninguna posición propia — asume que
    quien lo cargue lo va a colgar (mismo primitivo `NiNode::AttachChild`
    ya usado en este proyecto para el arma equipada, `CLAUDE.md`) de un
    nodo que YA está en el sitio correcto (el `magicNode` que resuelve
    `ActorMagicCaster` según la mano — ver más arriba), y hereda esa
    posición/orientación automáticamente por jerarquía de escena, sin
    ningún cálculo propio.
  - **El "envolver" es solo la forma del volumen emisor, centrada en ese
    mismo origen** — decodificados los valores reales: en
    `lightningstormhandeffects.nif`, `NiPSysCylinderEmitter` con radio
    ~1.5-2.5 y altura 16-24 (unidades Skyrim); en `shockhandeffects.nif`,
    `NiPSysBoxEmitter` de ~5-26 de ancho por ~2-5 de alto/profundo. Son
    volúmenes pequeños centrados en `(0,0,0)`, del tamaño aproximado de
    un puño/antebrazo — de ahí que "envuelva" la mano visualmente una vez
    colgado del hueso correcto: las partículas nacen dentro de ese
    volumen pequeño alrededor del punto de enganche, no porque el NIF
    conozca la forma de la mano.
  - **Conclusión práctica para construir uno**: el NIF de un handeffect es
    tan agnóstico de "ser un efecto de mano" como el NIF del arma es
    agnóstico de "estar equipada" — es un `BSFadeNode` raíz en el origen,
    con el mismo tipo de contenido ya documentado arriba (núcleo de
    brillo + emisores de partículas con `BSEffectShaderProperty`,
    fases `NiControllerSequence` si se quiere el ciclo
    Intro/Charge/Ready/Cast) dimensionado a mano alzada/puño. Quién lo
    clasifica como "de mano" y quién decide a qué hueso colgarlo es
    trabajo del Magic Effect/`ActorMagicCaster` — no vive en el fichero.
- **Brillo/emisivo en un arma** — visto en
  `_reference/Kratos Combat - 2.8.6a/meshes/hmmWorks/Mjolnir/Blade_of_Chaos_custom.nif`
  (mod publicado, plantilla obligatoria del proyecto): un
  `BSEffectShaderProperty` propio (distinto del `BSLightingShaderProperty`
  del material base) con texturas emisivas dedicadas
  (`blades_of_chaos_em.dds`, `blades_of_chaos_em_2.dds`) y un
  `NiAlphaProperty` para el blending, aplicado sobre una malla `BSTriShape`
  **separada y superpuesta** (nombrada `blade_em` / `custom burning`) en vez
  de modificar el shader de la malla base. La variante sin el sufijo
  `_custom` no tiene esa malla/shader de efecto adicional — es la comparación
  útil para ver exactamente qué añade el efecto.
- **Texture set**: `BSShaderTextureSet` (referenciado solo desde
  `BSLightingShaderProperty`/`BSShaderPPLightingProperty` — **no** desde
  `BSEffectShaderProperty`, que lleva sus propios campos de texto
  directos: `Source Texture`, `Greyscale Texture`, `Env Map Texture`,
  `Normal Texture`, `Env Mask Texture`) tiene, según `nif.xml`, orden fijo
  de slots: `0` Diffuse, `1` Normal/Gloss, `2` Glow (`SLSF2_Glow_Map`) /
  Skin / Hair / Rim light, `3` Height/Parallax, `4` Environment, `5`
  Environment Mask, `6` Subsurface (Multilayer Parallax), `7` Back
  Lighting Map (`SLSF2_Back_Lighting`) — fuente 3, extracto en
  `references/nifxml-excerpts.md`. Los slots 6-7 solo se usan si el
  shader tiene el flag correspondiente activo; con menos de 8 rutas
  presentes, los índices bajos (0-2) son los que casi siempre importan.
- **Giro/animación horneada en el propio NIF, controlada por código**:
  patrón ya verificado en el juego en este mismo proyecto — usa tal cual lo
  descrito en `CLAUDE.md` (`NiTransformController` + nodo hijo dedicado +
  `Start()`/`Stop()`), no el patrón siguiente, salvo que el usuario pida
  explícitamente replicar Kratos.
- **Patrón alternativo real, visto en Kratos** (no usado por este proyecto,
  documentado solo como referencia): `Projectile_LeviathanAxe{A,H,L}.nif`
  usan `NiControllerManager` con un `NiMultiTargetTransformController` y una
  o varias `NiControllerSequence` nombradas (`mIdle` en la variante A;
  `mBegin`/`mLoop`/`mEnd` en H y L, para fases de vuelo distintas) en vez de
  un `NiTransformController` suelto. Si el usuario pide replicar Kratos
  punto por punto para esto, usa este patrón — si no, no lo mezcles con el
  ya verificado del proyecto sin que lo pida.
- **UVs en movimiento (textura que "scrollea", p. ej. energía/fuego/agua
  que fluye) — confirmado con valores reales, ya no solo el spec.**
  Mecanismo: `BSEffectShaderPropertyFloatController`/
  `BSLightingShaderPropertyFloatController` con `Controlled Variable` =
  `U Offset`/`V Offset` (también existen `U Scale`/`V Scale`); cadena
  `Interpolator` → `NiFloatInterpolator` → `Data` → `NiFloatData` con las
  claves de animación — eso sigue siendo lectura de `nif.xml`. Pero ahora,
  decodificando el campo de verdad (método (c), 2026-08-07) en varios NIFs
  reales:
  - `stafflightningproj.nif` (Storm Calling, mod publicado): de sus 5
    controladores `BSEffectShaderPropertyFloatController`, **3 animan
    `V Offset` (scroll de UV real)** y 2 animan `Alpha Transparency`
    (fundido) — confirma que Storm Calling sí usa scroll de UV para el
    efecto de rayo, combinado con fundido de alpha.
  - `lightspellprojectile.nif` (**malla vanilla de Skyrim**, el proyectil
    de hechizo de luz del propio juego base): sus 2 controladores animan
    `V Offset` y `U Offset` — **confirma que el propio Skyrim usa esta
    técnica** en un asset del juego base, no solo mods.
  - Contraste: en el trail de Precision (`AttackTrail(Magic).nif`) el
    mismo tipo de controlador (`BSLightingShaderPropertyFloatController`
    ahí) anima `Refraction Strength`, no UV — mismo bloque, uso distinto
    según el efecto. No asumas cuál de los dos sin decodificarlo si
    importa para el caso concreto.
  
  Hay un mecanismo más antiguo, `NiUVController`/`NiUVData`, pero el
  propio `nif.xml` lo marca `DEPRECATED (pre-10.1), REMOVED (20.3)` y no
  aparece en ninguno de los NIFs reales de este repo — no lo recomiendes
  sin que el usuario lo pida.
- **Partículas nativas** (`NiParticleSystem` / `BSStripParticleSystem` +
  modifiers) — **ahora con ejemplos reales**, no solo formato: los NIFs de
  `_reference/Nif examples/meshes/stormcalling*/` (mods publicados de
  magia de rayos) combinan, en producción, `NiParticleSystem` + `NiPSysData`
  + un emisor (`NiPSysMeshEmitter` o `NiPSysCylinderEmitter`) +
  `NiPSysSpawnModifier` + `NiPSysAgeDeathModifier` + `NiPSysPositionModifier`
  + `NiPSysBoundUpdateModifier` + `NiPSysGravityModifier` +
  `NiPSysDragModifier` (una instancia por eje, nombradas literalmente
  `"NiPSysDragModifier(X-Axis)"`/`"(Y-Axis)"`/`"(Z-Axis)"` en el propio
  fichero) + `NiPSysRotationModifier`, más los controladores
  `NiPSysEmitterCtlr`, `NiPSysUpdateCtlr` y `NiPSysModifierActiveCtlr` — el
  conjunto que ya recoge `references/nifxml-excerpts.md` desde el spec,
  ahora confirmado como combinación real y no solo teórica.
  Para el estilo **"tira/estela"** de Skyrim (justo lo relevante para una
  futura estela del arma en vuelo): `boltescapebodyfx.nif`,
  `lightningsplashhazard(new).nif` y `maelstrom_hazardparticles.nif` usan
  `BSStripParticleSystem` + `BSStripPSysData` +
  `BSPSysStripUpdateModifier` juntos de verdad — confirma la combinación
  que antes solo estaba en el spec.
  **Tipos adicionales vistos en estos NIFs reales, ya extraídos de
  `nif.xml`** (campos exactos en `references/nifxml-excerpts.md`):
  `BSPSysScaleModifier` (lista de escalas por edad de la partícula),
  `BSPSysSimpleColorModifier` (color/alpha en 3 tramos con % de
  entrada/salida), `BSPSysSubTexModifier` (anima qué recuadro de un atlas
  de textura se muestra — sprite sheet de partículas), `BSPSysLODModifier`
  (reduce emisión/tamaño según distancia), `NiPSysEmitterSpeedCtlr` (anima
  `Speed` del emisor con el tiempo), `NiPSysBombModifier` (fuerza
  explosiva), `BSLagBoneController` (retrasa un hueso respecto al actor —
  no es de partículas, es física de hueso tipo "cola"/capa).
  `BSProceduralLightningController` (visto en `stafflightningproj.nif`) sí
  es justo lo que sugiere el nombre: genera geometría de rayo
  proceduralmente sobre mallas "dummy" mediante 9 interpoladores
  (generación, mutación, subdivisión, nº de ramas y su variación, longitud
  y su variación, ancho, offset de arco) — específico de efectos de rayo,
  no aplicable a un trail de arma sin más.

## Activación de partículas Bethesda vía `NiControllerManager` + graph variable (sesión larga, 2026-08-09 — `fxsparkfountaintoggle.nif`/`ThorMjolnirSparks.nif`)

Caso de estudio completo, con muchas rondas de prueba real en el juego y en
el preview de la Creation Kit — el patrón más importante que ha salido de
esta skill hasta ahora, porque es genérico a **cualquier NIF de partículas
"toggle" de Bethesda** (nombre típico `FXSparkFountainToggle*`,
`FX*Toggle*`), no solo a este fichero.

**El error de fondo, cometido y corregido dos veces en esta sesión (con dos
NIFs distintos): borrar `NiControllerManager` + sus `NiControllerSequence`
no es la forma de conseguir que un NIF de partículas se reproduzca solo.**
Aunque la cadena de controladores "real" del sistema de partículas
(`NiPSysUpdateCtlr`/`NiPSysEmitterCtlr`/etc., colgada directamente del
campo `Controller` del propio `NiParticleSystem`, independiente del
manager) quede estructuralmente intacta y con el flag `kActive` puesto,
**no se reproduce nada** — ni en el preview de la CK, ni en el juego real.
Confirmado sustituyendo también el mecanismo de carga (de
`RE::BSTempEffectParticle::Spawn` a un `Activator` real colocado con
`PlaceObjectAtMe`) para descartar que fuera el mecanismo de C++: con el
`.nif` editado (sin manager) seguía sin verse nada incluso con la
referencia colocada y confirmada correctamente.

**Por qué (diagnóstico verificado, no solo teoría)**: `NiControllerManager`
no tiene ningún campo en el formato que diga "reproduce esta secuencia por
defecto" (`Cumulative`/`Num Controller Sequences`/`Controller
Sequences`/`Object Palette`, ver `nif.xml`) — la selección de qué secuencia
suena, y con qué peso, es **siempre externa**, vía una graph variable con
nombre (`fToggleBlend` en este fichero) que un script Papyrus adjunto al
`Activator` vanilla (`FXSetBlendVariableScript`, encontrado en
`Data\Scripts\Source\FXSetBlendVariableScript.psc` del propio juego)
escribe en su evento `OnLoad()`:

```papyrus
Event OnLoad()
    SetAnimationVariableFloat("fToggleBlend", myfToggleBlend)
EndEvent
```

Sin manager, no hay nada que seleccionar. Con manager pero sin que nadie
ponga esa variable, tampoco se selecciona nada — el resultado visual es el
mismo (nada) en los dos casos, lo que hace fácil confundir "el manager
sobra" con "falta activarlo". **La solución correcta: dejar el `.nif`
exactamente como vino (manager + secuencias intactas) y poner la graph
variable a mano** — o bien reproduciendo el script vanilla en una
referencia real colocada por el mod (recomendado si el objeto se coloca
como `Activator`/`TESObjectREFR`, ver la sección de C++ más abajo), o bien
(si de verdad hace falta que el NIF se autoactive sin ningún actor
Papyrus/C++ de por medio) explorando si `Object Palette`/algún flag
adicional del manager permite fijar una secuencia por defecto — no
investigado en esta sesión porque la vía de la graph variable resultó
suficiente.

### Estructura interna de un `NiControllerManager` con variantes ("Heavy"/"Light", `partA`/`partB`)

Cada `NiControllerSequence` (`partA`, `partB` en este fichero) tiene su
**propia** lista `Controlled Blocks`, con un `Interpolator`/`Data`
**dedicado** para cada parámetro animado (`NiPSysEmitterLifeSpanCtlr`,
`NiPSysEmitterInitialRadiusCtlr`, `NiPSysEmitterSpeedCtlr`,
`NiPSysEmitterCtlr`→`BirthRate`, `NiPSysEmitterCtlr`→`EmitterActive`) — **no
comparte los interpoladores** con la cadena "real"/directa del
`NiParticleSystem` (esos, los que cuelgan de `Controller` en el propio
`NiParticleSystem`, suelen ser `NiBlendFloatInterpolator`/`NiBlendBoolInterpolator`
con `Manager Controlled=true` y `Value` inválido — `-3.4e38`, el marcador de
"sin valor propio, lo rellena el manager en tiempo real" — no editar esos
directamente, nunca tienen efecto). **Para cambiar el valor real de un
parámetro (velocidad, tasa de nacimiento, vida, radio inicial...) de una
variante concreta, edita el `NiFloatData` de ESA secuencia** (localízalo con
`nif_sequence_decode.py`, columna `Interp=...`, sigue su `Data`). En este
fichero, valores reales decodificados del vanilla sin tocar:

| | `partA` ("Heavy") | `partB` (floja) |
|---|---|---|
| LifeSpan | 2.0 | 1.0 |
| InitialRadius | 2.0 | 1.0 |
| Speed | 300.0 | 90.0 |
| BirthRate | 30.0 | 9.0 |
| Cycle Type / Stop Time | LOOP / 0.333s | LOOP / 0.3s |

Ambas secuencias vienen con un **bucle muy corto (~0.3s)** — pensado para
mezclarse en tiempo real entre las dos según `fToggleBlend` varía
continuamente (uso vanilla: una fuente que "respira" entre floja y fuerte),
no para usarse a intensidad fija y bucle largo como necesita este proyecto.

### Trampa de las tangentes al alargar el bucle de una secuencia

Si se alarga `Stop Time` de una secuencia (p. ej. de 0.3s a 40s para que el
bucle casi no se note) hace falta **también** actualizar el `Time` de la
última clave de cada `NiFloatData` que uses — si se deja en el valor
antiguo, la interpolación queda indefinida/mantenida más allá de la última
clave según el motor, no necesariamente plana. Y aunque se actualice el
`Time` correctamente, si el tipo de interpolación es `QUADRATIC_KEY`
(Hermite, con tangentes `Forward`/`Backward` por clave), **las tangentes no
se reescalan solas** al mover las claves en el tiempo — una tangente
calculada para un tramo de 0.3s, aplicada ahora sobre un tramo de 40s,
sigue produciendo una curva real (no plana) aunque los dos extremos tengan
el mismo `Value`, y ese "bache" en medio del tramo puede leerse como un
pulso/parpadeo periódico si el valor afecta a birth rate/velocidad/vida.
Confirmado en el juego: con `Backward=-4800` sin corregir en la segunda
clave de `LifeSpan`, el efecto seguía pulsando pese a que `Speed`/`BirthRate`
ya estaban arregladas.

**Arreglo más robusto, no solo "poner las tangentes a 0"**: si el valor
debe ser constante durante todo el bucle, **reduce `Num Keys` a 1** (una
sola clave en `Time=0.0`) — con una sola clave no hay nada que interpolar,
el valor se mantiene fijo indefinidamente sin depender de que la duración
del bucle coincida con ningún `Time` de clave. Cambia también
`Interpolation` a `LINEAR_KEY` (con una sola clave da igual el tipo, pero
así no quedan campos `Forward`/`Backward` sueltos). Usa
`nif_keydata_decode.py` para confirmar antes/después.

### El límite de partículas simultáneas — `BS Max Vertices`, el campo más fácil de pasar por alto

**Síntoma característico, para reconocerlo sin tener que redescubrirlo**: al
subir el `Birth Rate` de un sistema, las partículas salen a ráfagas en vez
de en flujo continuo — un tramo "encendido" corto seguido de un tramo
"apagado" mucho más largo, en bucle regular. **El tramo apagado dura
aproximadamente lo mismo que el `Life Span` de la partícula**; el tramo
encendido dura aproximadamente `limite / BirthRate` segundos. Si el patrón
encaja con esa proporción, no es un problema de la secuencia/manager (ver
más arriba) — es este límite.

**Causa**: `NiPSysData` (la data del `NiParticleSystem`, no del emisor)
lleva un campo que en versiones antiguas del formato se llama `Num
Vertices` pero que, específicamente en Bethesda 20.2.0.7 (Skyrim SE/AE, la
macro `#BS202#` de `nif.xml`), se renombra a **`BS Max Vertices`** y pasa a
significar el número máximo de partículas vivas a la vez — no es un dato
sobre la malla (`NiPSysData` en BS202 no reserva vértices/normales/UVs
propios pese a llevar los flags `Has Vertices`/`Has Normals`/etc., ver el
comentario del propio `nif.xml`: "Vertices, Normals, Tangents, Colors, and
UV arrays do not have length for NiPSysData regardless of 'Num' or
booleans"). Si `Birth Rate × Life Span` (la demanda en régimen permanente)
supera este límite, el sistema se llena casi al instante y deja de nacer
nada hasta que las partículas más viejas mueran y liberen hueco —
exactamente el patrón de ráfagas descrito arriba. En
`fxsparkfountaintoggle.nif` vanilla, este límite venía en **`62`** —
suficiente para las tasas de nacimiento vanilla (9-30/s con vida 1-2s, unas
9-60 partículas en régimen permanente) pero no para valores mucho más
altos.

**Cómo verlo en NifSkope**: bloque `NiPSysData` (el hijo `Data` del
`NiParticleSystem`), dentro de su sub-estructura `Data`/`BS Max Vertices`
(puede aparecer etiquetado igual que `Num Vertices` en builds de NifSkope
con un `nif.xml` embebido más antiguo que no conozca el renombrado BS202 —
si ves `Num Vertices` en un `NiPSysData` de un NIF de Skyrim SE, es este
mismo campo). **Arreglo**: súbelo por encima de tu demanda real
(`BirthRate × LifeSpan`, con margen) — no hay motivo para no dejarlo
generoso (500, 1000...) salvo coste de rendimiento en sistemas con miles de
partículas reales, irrelevante para un VFX de arma.

**Decodificación real, verificada 2026-08-09**: ver `scripts/nif_psysdata_maxparticles.py`
— el offset hasta este campo (`Group ID` 4 bytes + `BS Max Vertices` ushort
2 bytes, justo al principio del bloque) se validó comprobando que los
campos que vienen varios pasos después en el mismo bloque (`Num Subtexture
Offsets`/`Subtexture Offsets`) salían con sentido real (una cuadrícula UV
4×4 coherente, offsets de 0.25 en patrón regular) en vez de basura — señal
de que el layout intermedio (incluido `BS Max Vertices`) está bien
calculado, no solo que "compila".

## Enganchar un VFX de partículas a un objeto en movimiento, por código (C++) — qué funciona y qué no

Fuera del ámbito estricto de "editar el `.nif`", pero surgido directamente
de intentar hacer visible un NIF de partículas Bethesda desde un plugin
SKSE — documentado aquí porque condiciona qué diseño de NIF tiene sentido
(p. ej., si vas a usar `BSTempEffectParticle`, un NIF que dependa de una
graph variable externa nunca va a funcionar, sea cual sea su contenido).
Tres mecanismos probados en el juego real, por este orden, cada uno
descartado con evidencia concreta antes de pasar al siguiente:

1. **`RE::BSTempEffectParticle::Spawn`** (carga un `.nif` suelto por ruta,
   sin formulario de por medio) — **descartado**: el modelo carga de
   verdad (confirmado con `particleObject` no nulo tras un margen real,
   comprobado con logging), pero nunca llega a renderizar nada, con
   ninguna combinación de `a_flags` (probado `0` y `7`, este último
   recuperado del historial de git de un intento anterior del propio
   proyecto) ni de escala. Sospecha razonada y no descartada: esta API no
   tiene grafo de animación propio (no es una `TESObjectREFR`, no hereda
   `IAnimationGraphManagerHolder`), así que no hay forma de poner la graph
   variable que un NIF de partículas Bethesda "toggle" necesita (ver
   sección anterior) — coherente con que el modelo cargue pero no se
   active nunca.
2. **`RE::TESObjectREFR::PlaceObjectAtMe` (Activator real) + `RE::NiNode::AttachChild`**
   sobre el hueso/nodo objetivo — **descartado con una prueba decisiva**:
   probado incluso enganchando, en vez del Activator de partículas, una
   copia de un objeto garantizado bueno (el arma que el proyecto ya
   renderiza sin problema en todo el resto del código) — tampoco se vio
   nada, sosteniendo la prueba 15+ segundos reales. Confirma que el fallo
   no es del Activator/NIF de partículas en absoluto, es del propio
   `AttachChild` para este uso. Sospecha razonada, no confirmada:
   `AttachChild` mueve el nodo 3D de verdad, pero la referencia
   (`TESObjectREFR`) puede seguir creyendo, a efectos de
   culling/streaming/visibilidad de alto nivel, que sigue en las
   coordenadas donde la colocó `PlaceObjectAtMe` originalmente — mismo tipo
   de desajuste "posición lógica vs. nodo 3D real" ya documentado en
   `CLAUDE.md` para Havok (`SetPosition`/`SetAngle` no actualizan el
   `bhkRigidBody` por sí solos), aquí quizás a nivel de renderizado.
3. **`PlaceObjectAtMe` + control manual por tick** (`SetPosition` +
   sincronizar Havok + refrescar el nodo visual cada ~16ms, el mismo patrón
   ya usado y probado en el proyecto para mover la réplica del arma) —
   **funciona, confirmado con la misma prueba del arma equipada**: sí se
   vio, siguiendo al objetivo con fluidez. Es el único de los tres que
   renderiza de forma fiable. Con el Activator real (en vez del arma de
   prueba) + la graph variable puesta a mano (ver siguiente punto), el
   efecto de partículas se vio por fin correctamente.

**Cómo poner la graph variable desde C++** (equivalente exacto al script
Papyrus `FXSetBlendVariableScript`, sin pasar por Papyrus): `RE::TESObjectREFR`
hereda `IAnimationGraphManagerHolder` (confirmado contra el header real,
`lib/commonlibsse-ng/include/RE/T/TESObjectREFR.h`), que expone
`SetGraphVariableFloat(const BSFixedString& a_variableName, float a_in)`
(`RE/I/IAnimationGraphManagerHolder.h`) — llamar esto sobre la referencia
recién colocada, justo después de confirmar que tiene 3D cargado (mismo
momento en que se pondría en modo Havok `kKeyframed`), reproduce
exactamente lo que el script hace en `OnLoad()`. Solo es posible sobre una
referencia real (`TESObjectREFR`) — es la razón concreta por la que el
intento 1 (`BSTempEffectParticle`) no podía funcionar para este tipo de
NIF, y otra razón más para preferir el mecanismo 3 sobre el 1 siempre que
el NIF dependa de una graph variable.

## Formato de salida al responder

Para cada afirmación concreta sobre estructura NIF:

```
**Afirmación:** [p. ej. "el shader de brillo va en un BSEffectShaderProperty separado"]
**Fuente:** CLAUDE.md ("Animación horneada...") | NIF real: <ruta> (inspeccionado con método a/b/c — especifica cuál: (a) solo confirma tipos de bloque presentes, (c) da el valor real de un campo) | <URL> (NifTools/CK wiki) | sin verificar — pendiente de comprobar en NifSkope/en el juego
```

Si el usuario pide un tipo de VFX sin ejemplo local ni fuente citable
disponible, dilo explícitamente y propón cómo verificarlo (buscar un NIF de
referencia adicional, consultar NifTools/CK wiki, probar en NifSkope y
reportar el resultado) en vez de improvisar una receta.
