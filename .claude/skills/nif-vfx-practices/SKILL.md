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
       `meshes/dlc01/weapons/crossbow/boltprojectile.nif`: mallas
       **vanilla** de Skyrim (proyectil de hechizo de luz, flecha órcica,
       virote de ballesta DLC) — la fuente más autorizada posible para
       "cómo lo hace Bethesda", con la salvedad de que son de una época
       distinta (formato/estilo pre-SSE en textura de nodos) y pueden no
       reflejar las convenciones más nuevas de `BSTriShape`/SSE al 100%.
     
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
