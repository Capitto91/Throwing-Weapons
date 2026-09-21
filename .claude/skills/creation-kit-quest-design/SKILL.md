---
name: creation-kit-quest-design
description: Ayuda a planificar e implementar una quest de Skyrim SE en la Creation Kit (Quest Data, Aliases, Stages, Objectives, scripting Papyrus, Packages, Scenes, Constructible Objects) sin inventar nombres de campo/checkbox ni comportamiento no documentado. Pensada en particular para una quest de recolectar materiales en distintas partes del mundo para poder fabricar/desbloquear un arma, pero aplica a cualquier quest. Úsala siempre que el usuario pida diseñar, planificar o implementar una quest, un fetch quest, un sistema de recolección de materiales, o cualquier trabajo dentro de la Creation Kit de Skyrim (aliases, quest stages, dialogue, scenes, COBJ/constructible objects), y también cuando se pida escribir, revisar o depurar código Papyrus (buenas prácticas, rendimiento, ciclo de vida de scripts de quest, fragments), incluso si no lo pide con la palabra "skill".
---

# Diseñar e implementar una quest de Skyrim SE en la Creation Kit, sin inventar

La Creation Kit tiene cientos de campos, checkboxes y comportamientos internos cuyos nombres
exactos, orden de pestañas y matices (qué necesita qué para funcionar) no se adivinan por
intuición — la memoria de entrenamiento sobre "cómo se hace una quest en Skyrim" mezcla tutoriales
de calidad muy distinta y de ediciones distintas del motor (Morrowind/Oblivion Construction Set,
Skyrim CK, Fallout 4 CK), y un nombre de campo que "suena bien" pero no es el real no da error de
compilación — el editor simplemente no lo tiene, o el usuario pierde tiempo buscándolo donde no
está.

**Regla rectora, igual que ya exige `CLAUDE.md` del proyecto para el documento de diseño del arma
y `verify-commonlibsse-api` para las APIs de CommonLibSSE-NG: para cualquier afirmación concreta
sobre un campo, checkbox, tipo de alias o comportamiento de la Creation Kit, cita de dónde sale.**
Si no puedes citarlo según la jerarquía de abajo, dilo explícitamente ("no verificado, esto es una
suposición mía") en vez de darlo por bueno. Esto aplica con más fuerza todavía a la parte de
**materiales/ubicaciones del mundo de Skyrim** que el usuario pida usar en la quest: un nombre de
mena, ingrediente o mazmorra inventado es fácil de detectar y rompe la inmersión de inmediato.

## Jerarquía de fuentes de verdad

1. **Ya verificado en este proyecto (`CLAUDE.md`)** — en particular, si esta quest va a vivir en
   el mismo plugin que ya usa este proyecto (`ThorMjolnirOAR.esp`, con flag ESL activo) o necesita
   que el plugin SKSE compruebe su progreso (p. ej. un `TESGlobal` que marque "arma desbloqueada"),
   aplica directamente la trampa ya documentada en `CLAUDE.md` → "Errores comunes a vigilar": un
   FormID de un plugin ESL solo tiene 12 bits reales (`0x000`-`0xFFF`) — hay que enmascararlo
   (quedarse con los últimos 3 dígitos hex) antes de usarlo en `RE::TESDataHandler::LookupForm` o
   en cualquier búsqueda por FormID+plugin desde C++. No es específico de esta skill, pero es el
   punto de contacto más probable entre "la quest que diseñes aquí" y "el código SKSE ya
   existente".
2. **`ck.uesp.net`** (antes `creationkit.com/wiki`) — el wiki oficial/de referencia de la Creation
   Kit. **Advertencia técnica confirmada en esta sesión**: todo el dominio está detrás de un
   challenge de Cloudflare que bloquea con `403 Forbidden` cualquier petición no-navegador —
   `WebFetch` y `curl` con user-agent de navegador completo fallan igual, en cualquier página
   probada. No es un caso puntual (ya se documentó el mismo bloqueo para una sola página en
   `nif-vfx-practices/SKILL.md`; aquí se confirma que es el dominio entero). **La única vía es
   `WebSearch` y citar el snippet devuelto** — nunca fetch completo de la página. Ya hay un extracto
   compilado y citado de varias páginas clave en
   `references/quest-mechanics-ck-wiki.md` — consúltalo primero antes de repetir búsquedas; si
   necesitas algo que no está ahí, usa `WebSearch` con el nombre exacto de la página
   (`ck.uesp.net "Nombre_De_La_Página"`) y añade lo nuevo a ese archivo si es información
   reutilizable. **Actualización 2026-09-20**: tampoco funcionan `skyrimck.uesp.net` (`403`),
   `creationkit.com` (página de mantenimiento de XWiki), `web.archive.org` (`429`, bloquea por bot)
   ni los foros de Bethesda `gamesas.com` (el dominio ya no les pertenece). Para el **texto** de una
   página de función/evento de la CK Wiki, la vía que sí funciona es `papyrus.bellcube.dev`
   (punto siguiente). Tabla completa de accesibilidad de fuentes de comunidad en
   `references/papyrus-community-practices.md`.
3. **`papyrus.bellcube.dev`** ("The Papyrus Index", aportada por el usuario) — índice de
   scripts/funciones/eventos de Papyrus (vanilla + SKSE + librerías de mods), organizado por
   juego. **Fetchable directo con `WebFetch`, sin el bloqueo de Cloudflare de `ck.uesp.net`**
   (confirmado en esta sesión sobre varias páginas) y cada página cita su fuente (CK Wiki,
   licencia CC BY-SA) — en la práctica, es la vía más rápida para conseguir la firma exacta de una
   función/evento de Papyrus sin pelear con `WebSearch`. Slug del juego para Skyrim SE:
   `skyrimse`. Patrón de URL:
   `https://papyrus.bellcube.dev/skyrimse/script/<scriptname>/function|event/<nombre>/`. Detalle
   completo, catálogo de scripts relevantes y hallazgos ya citados en
   `references/papyrus-bellcube-scripts.md` — consúltalo primero; si hace falta un
   script/función/evento nuevo, sigue el flujo de la última sección de ese archivo antes de
   recurrir a `WebSearch` contra `ck.uesp.net`.
4. **`en.uesp.net`** (UESP general — objetos, ubicaciones, quests vanilla ya publicadas, lore). A
   diferencia de `ck.uesp.net`, **este dominio SÍ es fetchable directo** (confirmado con `curl`
   normal, sin necesitar user-agent especial ni nada — devuelve `200`). Es la fuente fiable para
   cualquier dato concreto de materiales/ubicaciones del juego real (qué mena hay en qué mina, qué
   ingrediente suelta qué criatura, qué hace una quest vanilla paso a paso). Ya hay un extracto
   compilado en `references/materiales-mundo-abierto.md` (Ebony, Stalhrim, Daedra Heart, Aetherium
   Shard, Void Salts) — para un material que no esté ahí, descárgalo con `curl` a un fichero
   temporal (ver método abajo) en vez de fiarte de memoria.
5. **Mirrors de formato de registro en GitHub** (`github.com/TES5Edit/meta`, que replica la
   documentación de formato de fichero de UESP usada por los devs de xEdit) — **fetchable directo
   con `WebFetch`**, sin el bloqueo de `ck.uesp.net`. Útil para confirmar la estructura interna
   exacta de un registro (p. ej. `QUSTDef.wiki` para el registro `QUST`) cuando hace falta más
   precisión de la que da la UI del editor, o como segunda fuente que contraste lo que dice el
   wiki de la CK.
6. **El propio registro real, abierto en la Creation Kit o en xEdit/SSEEdit** — la fuente más
   fiable de todas cuando está disponible: si el usuario tiene el juego instalado con sus DLCs, la
   forma de confirmar con certeza absoluta cómo Bethesda construyó una quest concreta (qué
   aliases, qué stages, qué conditions) es abrir ese registro directamente, no asumirlo desde un
   walkthrough de UESP (que documenta la experiencia del jugador, no el árbol de aliases interno).
   Ver más abajo, sección "Lost to the Ages", para un caso concreto donde esto aplica.
7. **Nunca** des por buena de memoria de entrenamiento el nombre exacto de un campo/checkbox de la
   CK, un tipo de alias, una convención numérica, o el nombre/ubicación de un objeto real del
   juego, sin haber pasado por 1-6. Si ninguna aplica, dilo.
8. **Ojo con mezclar ediciones del motor**: `ck.uesp.net` aloja tutoriales tanto del CK de Skyrim
   como del de **Fallout 4** (namespaces separados, p. ej. "Fallout 4 Simple Fetch Quest
   Tutorial"). Son parecidos pero no idénticos — Fallout 4 añadió tipos de alias que no están
   confirmados para Skyrim (ver más abajo, "Reference Collection Alias"). Si una búsqueda trae
   contenido de Fallout 4, dilo explícitamente y no lo apliques a Skyrim sin verificar que existe
   igual aquí.

## Cómo descargar una página de `en.uesp.net` cuando el extracto cacheado no basta

Confirmado en esta sesión (Windows, Git Bash): `curl` normal funciona sin necesitar headers
especiales. Para convertir el HTML a texto legible sin depender de ninguna librería externa
(`bs4`/`html2text` no están instalados en este PC), usa Python puro con regex sobre
`<div id="mw-content-text">`. **Ojo con rutas en Windows**: si el script Python usa
`glob`/`open()` con una ruta estilo `/c/tmp/...` (la que entiende Git Bash), Python nativo de
Windows la malinterpreta y no encuentra nada sin dar error — usa `C:/tmp/...` (barras normales,
pero con la letra de unidad de Windows) dentro del propio código Python.

```bash
curl -s -A "Mozilla/5.0 (Windows NT 10.0; Win64; x64)" "https://en.uesp.net/wiki/Skyrim:NombreDePagina" -o "/c/tmp/pagina.html"
```

```python
import re, html
data = open('C:/tmp/pagina.html', encoding='utf-8', errors='ignore').read()
m = re.search(r'<div id="mw-content-text"[^>]*>(.*?)<div id="catlinks"', data, re.S)
content = m.group(1) if m else data
content = re.sub(r'<script.*?</script>', '', content, flags=re.S)
content = re.sub(r'<style.*?</style>', '', content, flags=re.S)
content = re.sub(r'<[^>]+>', '\n', content)
content = html.unescape(content)
lines = [l.strip() for l in content.split('\n') if l.strip()]
open('C:/tmp/pagina.txt', 'w', encoding='utf-8').write('\n'.join(lines))
```

## Visión general del formulario Quest

Ver `references/quest-mechanics-ck-wiki.md` para el detalle completo y las citas exactas. Resumen:
el formulario se organiza en pestañas — **Data**, **Objectives**, **Aliases**, **Dialogue Views**,
scripts/fragments Papyrus, y una lista de **Scenes** asociadas. El orden recomendado para
construir una quest desde cero (deducido de que las Stages y Objectives referencian Aliases, así
que las Aliases tienen que existir primero):

1. **Planificar en papel primero** — el propio tutorial oficial de Bethesda ("Planning the Quest")
   empieza así, no abriendo el editor: decidir la secuencia de eventos de la quest como frases del
   tipo "el jugador ha hecho X" antes de tocar ningún stage.
2. Crear el formulario Quest y rellenar la pestaña **Data** (Priority, Quest Type, Start Game
   Enabled).
3. Definir las **Aliases** necesarias (una por cada material/lugar/NPC que la quest necesite
   referenciar).
4. Definir **Stages** y, dentro de cada stage con lógica, al menos un **stage item** con su script
   fragment y/o log entry.
5. Definir **Objectives**, apuntando cada uno a la Alias correspondiente como target.
6. Si hace falta diálogo o una escena coordinada (p. ej. el momento de fabricar el arma), añadir
   **Dialogue Views**/**Scenes** al final, cuando ya existen las aliases y stages a los que van a
   enganchar.

## El mecanismo central para "un material en cada parte del mundo": Quest Aliases

Esto es lo más importante de toda la skill para el caso de uso del usuario. Ver el detalle
completo y citado en `references/quest-mechanics-ck-wiki.md`, sección "Quest Alias Tab". Resumen
aplicado al caso de "recolectar N materiales distintos":

- **Cada material necesita su propia Reference Alias** — no hay (no está confirmado que exista)
  un tipo de alias de "colección" en el CK de Skyrim que agrupe varias referencias bajo una sola
  alias. Eso sí existe en el CK de **Fallout 4** ("Reference Collection Alias"), pero no se ha
  encontrado documentación de que Skyrim lo tenga — no lo asumas.
- Los tipos de "fill" verificados para rellenar una alias al arrancar la quest son: **Specific
  Reference** (una referencia fija del mundo), **Unique Actor** (requiere que ese actor tenga
  Persist Location asignado), **Location Alias + Location Ref Type** (busca dentro de otra alias
  de tipo Location ya definida más arriba en la lista), **External Quest Alias** (reutiliza la
  alias de otra quest) y **Create Reference** (crea una referencia nueva a partir de un base
  object al arrancar).
- Para materiales colocados a mano en el mundo (una mena concreta en una mina concreta, un
  cofre con el fragmento del arma dentro de una mazmorra), el patrón natural es **Specific
  Reference**: coloca el objeto en el editor de celdas donde corresponda, luego asígnalo como
  Specific Reference de su Alias.
- Cada Alias puede llevar su propio script (`ReferenceAlias`), sus propias Conditions y su propio
  inventario/keywords — así que la lógica de "¿ha cogido el jugador este material?" puede vivir en
  el script de la alias en vez de amontonarse toda en el Quest script.

## Stages, Objectives y el ciclo de scripting

Ver `references/quest-mechanics-ck-wiki.md` para el detalle citado. Puntos clave a tener en cuenta
al diseñar la quest de recolección:

- Numerar los stages en incrementos de 10 — es la recomendación explícita del tutorial oficial de
  Bethesda (no solo costumbre), para poder insertar stages intermedios después sin renumerar.
- Cada stage con script/log entry necesita al menos un **stage item**.
- Cada Objective apunta a una Alias como target — así se pinta el marcador en el mapa/compass del
  material o lugar correspondiente mientras ese objective está activo. Para "ve a buscar el
  material X", el target del objective sería la Alias de ese material (o de su ubicación).
- `SetStage`/`GetStage`/`SetObjectiveDisplayed` son las funciones Papyrus estándar para mover la
  quest de un stage a otro y mostrar/ocultar objectives.
- **Idea opcional para el futuro (anotada 2026-09-21, sin adoptar en ningún script actual)**: para
  decidir "primera vez" o un prerrequisito, preferir `GetStageDone(n)` a rangos de `GetStage()`.
  `GetStage()` devuelve el stage completado **más alto** (comentario de `Quest.psc` vanilla), así que
  una condición como `< 12` o `15 ≤ stage < 25` depende de la numeración y se rompe si más adelante se
  cuela un stage alto antes de tiempo (riesgo R5 de `QUEST-DESIGN.md`). El script vanilla
  `defaultsetStageTrigSCRIPT` lo hace así para su prerrequisito opcional (`prereqStageOPT` con
  `getStageDone`) y `CAP_ThorMjolnir_Trigger_Forge` ya lo usa. `CAP_ThorMjolnir_Trigger_AtronachAtack`
  sigue con rangos de `GetStage()`: funciona hoy (según `QUEST-DESIGN.md`, el stage 25 lo pone ese
  mismo trigger) y el usuario decidió no cambiarlo.
- Detectar "el jugador ha recogido el material" es, según el patrón de comunidad más citado (no
  confirmado como texto literal de un tutorial oficial — dejarlo claro si se implementa así):
  condicionar con `GetItemCount()` y avanzar de stage cuando se cumple. Sitios típicos donde
  evaluar esto: el evento `OnItemAdded` de la alias del jugador, o el fragment de un stage de
  "comprobación".

### Si quieres que la recolección sea más dinámica que "N sitios fijos" (opcional, no obligatorio)

El sistema de **Radiant Quests** de Bethesda (`Bethesda_Tutorial_Radiant_Quests`, ver referencia)
introduce el concepto de **"seed alias"**: una alias inicial de la que dependen las demás (en su
ejemplo, elegir un ladrón determina también qué mazmorra y qué marcador de mapa usar). Si en algún
momento se quisiera que el juego eligiera dinámicamente en qué mina/mazmorra aparece cada
material (en vez de fijarlo siempre a mano), este es el sistema documentado para ello — pero para
una primera versión, **N Aliases con Specific Reference fijada a mano es más simple, más fácil de
depurar, y suficiente** si el número de materiales es pequeño y conocido de antemano.

## El paso final: fabricar el arma (Constructible Object)

Ver `references/quest-mechanics-ck-wiki.md`, sección COBJ, para el detalle citado. Resumen: un
`Constructible Object` define **Created Object** (el arma), **Workbench Keyword** (qué estación lo
habilita — forja, mesa de trabajo, etc.), una lista de **Items** requeridos con su **cantidad**
cada uno (aquí van los N materiales recolectados), y **Conditions** — es aquí donde se controla
que la receta solo aparezca disponible una vez completada la recolección (p. ej. condicionando
contra `GetStage` de esta quest, o contra un `Global` que la propia quest ponga a 1 al terminar).
El flujo práctico habitual es duplicar una receta existente parecida como plantilla en vez de crear
el registro desde cero.

## Scripts vanilla de Papyrus reutilizables — `papyrus.bellcube.dev`

Apartado añadido 2026-08-31 a petición del usuario, que aportó el sitio. Detalle completo,
catálogo de scripts y citas exactas en `references/papyrus-bellcube-scripts.md` — resumen de lo
más directamente aplicable a los 5 hitos de la quest en curso:

- **Hito 1 (llegar al altar arranca la quest)**: `ObjectReference.OnTriggerEnter(ObjectReference
  akActionRef)` confirmado con matices nuevos (puede llegar desordenado respecto a
  `OnTriggerLeave`, no detecta bien actores muertos/NPCs en puertas de teletransporte, y si el
  trigger descarga/recarga geometría puede disparar el evento varias veces — mitigación citada:
  `TranslateToRef()` en vez de desplazamientos normales). Si en algún momento se prefiere la
  Opción B (Story Manager) en vez del Trigger Box ya decidido, hay un dato nuevo: `Quest` tiene su
  propio evento `OnStoryChangeLocation(ObjectReference akActor, Location akOldLocation, Location
  akNewLocation)` — con el Event "Change Location Event" asignado en la pestaña Data, el propio
  Quest script recibe el actor/location vieja/location nueva ya resueltos, sin pasar por
  `GetEventData()`.
- **Hito 2 (leer el objeto X → notificación en pantalla)**: resuelto el hueco que había quedado
  abierto — `ObjectReference.OnRead()` (heredado por `Book`, sin evento propio necesario en
  `Book`) salta al abrirse la interfaz de lectura, y `Debug.Notification(string)` es la función
  exacta para el texto en pantalla (con una trampa de caracteres `<`/`>` documentada, evitarlos en
  el texto).
- **Hito 3 (detectar recogida de los 3 materiales)**: `GetItemCount(Form akItem)` confirmado como
  función real de `ObjectReference`/`Actor` (ya se citaba como patrón de comunidad; ahora con su
  página de documentación propia y sus trampas: falla con keywords sobre un actor, no fiable con
  leveled lists, devuelve `1` de forma no intuitiva si se llama desde fuera de la celda de la
  referencia sobre un form de una leveled list).
- **Hito 4 (forjar el arma)**: `ConstructibleObject` tiene script propio con 11 funciones
  (`GetResult`/`SetResult`, `GetNumIngredients`, `GetNthIngredient(Quantity)`,
  `GetWorkbenchKeyword`, etc.) — pero, según esa única página, **requieren SKSE** para funcionar
  (no verificado cruzado con otra fuente); no hacen falta para el flujo normal de "duplicar una
  receta en el editor y editar campos", solo si se quisiera leer/modificar una receta COBJ desde
  Papyrus en tiempo de ejecución. Si se opta por una Scene para este momento, `Scene` tiene
  `Start`/`ForceStart`/`Stop`/`IsPlaying`/`GetOwningQuest`.
- **Aliases**: `ReferenceAlias.ForceRefTo(ObjectReference akNewRef)` confirmado — útil si algún
  día se quiere rellenar una alias de material dinámicamente (p. ej. desde el propio plugin SKSE)
  en vez de fijarla a mano como Specific Reference; trampas documentadas: no vacía la alias (usar
  `Clear()` para eso), puede romper el package de un actor si se llama repetidamente durante una
  Scene activa, y no espera a que el cambio se refleje antes de devolver el control.

## Convenciones de scripting Papyrus (rendimiento/estilo, no específico de quests)

Añadido 2026-08-31. No hay ninguna skill dedicada solo a esto — lo que hay verificado vive aquí,
en el contexto de los scripts que ya hemos escrito para esta quest (ver también, justo debajo, la
checklist de buenas prácticas de código). Fuente:
`https://wiki.beyondskyrim.org/wiki/Arcane_University:Scripting_Best_Practices` (proyecto Beyond
Skyrim, guía de convenciones de scripting para su equipo — no es documentación oficial de
Bethesda/CK Wiki, pero sí una fuente de comunidad seria y ampliamente citada; tratarla como tal).

- **Evitar `Game.GetPlayer()` repetido**: *"almost always better to use and autofill an Actor
  property PlayerREF instead"*. Patrón: `Actor Property PlayerREF Auto`, rellenada con
  **Auto-Fill** en la ventana de properties (no a mano) — el motor la resuelve al jugador sola.
  Motivo: `Game.GetPlayer()` es una llamada nativa con coste, evitable si ya tienes una property
  cacheada.
- **Cuidado con properties de `ObjectReference`/`Actor` en general**: *"if you make an
  ObjectReference or an Actor a property, that ref will become persistent (always loaded)"* — el
  jugador no tiene este problema (siempre está cargado igualmente), pero cualquier otra referencia
  del mundo (un material, un NPC, la forja) sí lo arrastraría si se le pone una property directa
  apuntándola. Es la razón de fondo, más allá de la conveniencia de diseño, por la que este
  proyecto usa **Reference Aliases** (ver más arriba, "El mecanismo central...") para todo lo que
  no sea el jugador — una Alias no fuerza esa persistencia permanente de la misma forma que una
  property suelta.
- Ejemplo aplicado ya en esta quest: `CAP_ThorMjolnir_RosettaStoneActivator` usa
  `PlayerREF` (Actor Property, Auto-Fill) en vez de `Game.GetPlayer()` dentro de `OnActivate`.

## Checklist de buenas prácticas al escribir/revisar un script Papyrus

Añadido 2026-09-20 a petición del usuario y ampliado el mismo día con fuentes de comunidad. Bases:
la wiki de `fireundubh` (`references/papyrus-anti-patterns-fireundubh.md`) y guías, hilos y artículos
de la comunidad + páginas de la CK Wiki (`references/papyrus-community-practices.md`). **En esas
referencias cada afirmación lleva una marca de confianza** (✅ verificado aquí con el compilador o los
`.psc` vanilla / 📖 CK Wiki leída / 🗣️ comunidad / ⚠️ sin verificar). Aquí solo van las reglas que
sobreviven a esa verificación: si una no queda clara o quieres su fuente, ir allí.

**Dos advertencias transversales** (mismo espíritu que el punto 8 de la jerarquía de fuentes):
- **La wiki de fireundubh mezcla Fallout 4 y Skyrim sin avisar.** Comprobado con el compilador de
  Skyrim SE: `Property … Auto Const` → error `Unknown user flag Const` (usar `Auto` o
  `AutoReadOnly`); `new T[0]` y `.Add()` no existen (arrays de tamaño fijo, **1–128**); el workaround
  de `Activate(PlayerREF)` y `AddInventoryEventFilter(None)` son de Fallout 4. No copiar código de
  ahí tal cual.
- **Ante una duda de sintaxis o de qué hace el compilador: compilar una prueba mínima, no adivinar.**
  Compilador y `Scripts\Source` vanilla ya localizados en este PC; comando, trampas de PowerShell y
  `-keepasm` (bytecode legible) en `papyrus-anti-patterns-fireundubh.md`, sección "Cómo repetir una
  prueba así".

### A. Corrección — fallos que el compilador de Skyrim deja pasar sin avisar

1. **Toda ruta de una función con tipo de retorno acaba en `Return`.** ✅ No hay aviso (`0
   warning(s)`) y el bytecode queda sin `RETURN`. Si basta, devolver la expresión directa:
   `Return !akRef.Is3DLoaded()`.
2. **Validar `None` antes de llamar a un método o de usar un argumento**: `If kItem && DummyRef`.
   Según la fuente, un `None` no crashea pero aborta la llamada y se pierde el efecto en silencio.
   ✅ `&&`/`||` **hacen cortocircuito** (bytecode: `JUMPF` salta la llamada), así que
   `If R && R.IsDisabled()` es seguro.
3. **Validar arrays antes de indexar** (`If Arr != None`, `i < Arr.Length`), sobre todo properties
   de array que puedan quedar sin rellenar en la CK. ✅ Máximo 128 elementos por array.
4. **Todo `While` con salida anticipada.** ✅ No existe `Break` (`variable Break is undefined`): usar
   un flag `Bool bBreak` en la condición (`While (i < n) && !bBreak`). Un `While` que no duerme
   consume tiempo de VM; ~100 hilos vivos a la vez → volcado de pilas (🗣️ cita atribuida a SmkViper).
5. **Comprobar el denominador ≠ 0** antes de `/` o `%`.
6. **Alias**: ✅ `ReferenceAlias` no tiene `Enable()` (`Enable is not a function or does not exist`):
   `Al.GetReference().Enable()`. Comprobar `None` tras `GetReference()`. En un fragment la property es
   `Alias_<NombreExactoDelAlias>`: un nombre mal escrito da `variable … is undefined`.
7. **`AddInventoryEventFilter`** (📖 CK Wiki): los filtros **se acumulan** (quitar antes de
   sustituir), una `FormList` **no entra en listas anidadas**, y Skyrim **no admite `None`** (usar una
   `FormList` vacía; ✅ compila sin aviso, así que el compilador no lo delata).

### B. Rendimiento

8. **Eventos antes que sondeo**; con `RegisterForSingleUpdate`, **re-registrar al final** del
   manejador y llamar antes a `UnregisterForUpdate()` si puede haber un registro pendiente (📖 CK
   Wiki: *"or strange behavior may result"*). El intervalo no cuenta con un menú abierto. Los `Wait`
   largos, mejor evitarlos: `Utility.Wait` es latente, en tiempo real, **no preciso** y no avanza en
   menús (📖).
9. **Micro-optimizaciones gratuitas, siempre** (✅ verificadas en bytecode): `Utility.Wait(1.0)` y
   `f > 1.0` (con `1` se inserta un `CAST` en ejecución), `If b` / `If !b` (no `b == true` /
   `b != true`) y una property `PlayerREF` en vez de `Game.GetPlayer()`. Las funciones abreviadas
   (`GetAV`…) y de conveniencia (`GetValueInt`, `GetActorRef`, `GetRef`) son **wrappers Papyrus**:
   evitarlas solo en rutas calientes (bucles, `OnUpdate`, eventos frecuentes). El propio código vanilla
   las usa, así que **en código de un solo disparo (un fragment de stage) manda la legibilidad**.
10. **Evitar bucles anidados** (coste multiplicativo); preferir `FormList.HasForm(kItem)` a recorrer
    una lista dentro de otro bucle.
11. **Contexto que ayuda a decidir** (🗣️): Papyrus tiene ~1,2 ms de presupuesto por frame,
    compartido: un script pesado no baja el FPS pero **retrasa a los demás**. Un *stack dump* es de solo
    lectura y no daña nada, pero es síntoma de sobrecarga. No tocar los `.ini` de Papyrus como
    "arreglo".

### C. Ciclo de vida y guardado

12. **`OnPlayerLoadGame` no se dispara desde un Quest script.** ✅ Está declarado en `ReferenceAlias`
    y `Actor` vanilla, **no en `Quest`**; en un `extends Quest` compila sin aviso pero (🗣️) nunca
    salta. Patrón de comunidad (⚠️ no probado en el juego): `OnInit` del quest para la primera vez, un
    **ReferenceAlias del jugador** con `OnPlayerLoadGame` para cada carga, y una property de
    **versión** para migrar tras actualizar el script.
    - **`OnLoad` no es fiable al cargar una partida.** 📖 Nota de la CK Wiki (vía bellcube): *"This
      event doesn't fire reliably for references that load 3D while or immediately after the player
      loads a savegame"*; tampoco salta para objetos deshabilitados ni en un script de alias del
      jugador. ✅ Comprobado aquí (2026-09-21): un `OnLoad` con `Debug.Trace` en
      `AtronachVulnerability` no dejó ni una línea en `Papyrus.0.log` tras guardar, cerrar el juego y
      cargar. **No usarlo para resetear estado transitorio**; comprobarlo dentro de un `OnUpdate` (la
      cadena de `RegisterForSingleUpdate` sí sobrevivió al guardado: el atronach volvió solo a ser
      invulnerable, observado en el juego) o con `OnPlayerLoadGame` desde el alias del jugador.
    - **Un plazo con `Utility.GetCurrentRealTime()` no sobrevive entre sesiones.** 📖 Cuenta segundos
      desde que se lanzó el juego, pero una variable de script sí se guarda. Observado 2026-09-21: una
      fase de 15 s guardada abierta duró "un buen rato" tras reiniciar el juego. Validar en el propio
      manejador: un plazo válido nunca está más lejos de "ahora" que la duración máxima (+ un margen);
      si lo está, viene de otra sesión y se descarta.
13. **SEQ**: solo si la quest es *Start Game Enabled* (📖: sin el `.seq` fallan diálogo y scenes;
    regenerarlo al añadir/quitar una quest SGE). Esta quest arranca por Trigger Box: no lo necesita.
14. **Cambiar la estructura de un script ya presente en un save** (quitar o renombrar properties)
    deja avisos en el log (⚠️ CK Wiki, snippet): probar en un save anterior a ese script. Las
    properties `ObjectReference`/`Actor` fuerzan persistencia: usar Aliases (ver arriba).

### D. Fragments

15. **Fragment = una sola llamada** al script de la quest, con la lógica y los `Debug.Trace` en el
    script (⚠️ inferencia mía, coherente con la práctica ya seguida). ✅ La CK genera `kmyQuest` ya
    tipado como tu script de quest (`;BEGIN AUTOCAST TYPE`), y la zona *"Do not edit anything between
    this and the end comment"* no se toca. Un `QF_*` extiende `Quest` (`SetStage` directo); en un
    fragment de **diálogo** (`TIF_*`, extiende `TopicInfo`) hace falta `GetOwningQuest()`.

### E. Estilo y mantenimiento

16. **Funciones `Bool`: devolver la expresión directamente** en vez de `If cond Return True EndIf /
    Return False`; separar en varios `If` solo si cada condición dispara una acción distinta o la
    expresión es ilegible. *(fireundubh se contradice entre estas dos reglas; esta resolución es
    decisión propia, no de la fuente.)*
17. **Nombres**: solo los **parámetros** tienen convención firme de Bethesda (`a` + tipo: `akRef`,
    `abFlag`, `aiCount`, `afValue`, `asName`). Para locales y variables de script no hay estándar
    (🗣️ fireundubh, AFK Mods 2016): elegir una convención y aplicarla **entera y de forma
    consistente**; nada de variables de una letra. Tendencias: properties en UpperCamelCase,
    locales en lowerCamelCase, `Alias_` en properties de ReferenceAlias.
18. **Sin código muerto**: borrar funciones y properties sin usar (las properties sin usar avisan en
    el log); no dejar código viejo comentado dentro del `.psc`; no guardar en una variable una
    property que solo se usa en una rama.

### F. Depuración y perfilado

19. `Debug.Trace(texto, severidad)` (✅ firma en `Debug.psc`; convención de prefijo en la sección de
    instrumentación, más abajo). Para medir en serio: `bEnableProfiling=1`,
    `Debug.StartScriptProfiling("Script")` (se llama **desde otro script**) o
    `StartStackProfiling()`; **quitar las llamadas de perfilado antes de distribuir** (🗣️ artículo de
    Nexus). El logging puede costar rendimiento: apagarlo cuando no haga falta.
20. **Herramientas opcionales sin probar** (⚠️): *Papyrus Linter* (Nexus 189862, MIT, "atrapa lo que el
    compilador de la CK deja pasar"; su página lleva la etiqueta "AI-Generated Content") y la
    extensión `joelday/papyrus-lang` de VS Code. No recomendadas sin probarlas.

## Instrumentación de depuración en scripts nuevos — log, no notificaciones en pantalla

Añadido 2026-09-13, convención decidida por el usuario mientras depurábamos el ciclo del Atronach
de Tormenta (Material 2, el Rayo). **Cualquier script Papyrus nuevo que se monte para probar una
mecánica todavía no verificada en el juego debe llevar sus puntos de control con
`Debug.Trace`, no `Debug.Notification`** — así el propio Claude puede leer el resultado
directamente del archivo de log en la siguiente vuelta de la conversación, en vez de depender de
que el usuario transcriba a mano lo que ha visto salir en pantalla.

- **Requisito previo, comprobar antes de asumir que el log va a tener algo**: Papyrus logging
  tiene que estar activo en el `.ini` real que usa el perfil de MO2 en marcha — no necesariamente
  el de `Documents\My Games\Skyrim Special Edition\` a pelo, si el modlist usa inis por perfil.
  Confirmado en este proyecto en
  `D:\Modlists\SME\profiles\Skyrim Modding Essentials\skyrimcustom.ini`, sección `[Papyrus]`:
  `bEnableLogging=1`, `bLoadDebugInformation=1`, `bEnableTrace=1` — si en algún momento el log sale
  vacío pese a tener `Debug.Trace` en el script, esto es lo primero a revisar, no el script.
- El archivo real es `Documents\My Games\Skyrim Special Edition\Logs\Script\Papyrus.0.log` (ruta
  de Windows normal, no dentro de la carpeta `overwrite` de MO2).
- **Convención de formato de línea**: `Debug.Trace("[NombreDelMod][NombreCortoDelScript]
  mensaje")` — el prefijo entre corchetes deja grepear fácilmente qué script generó cada línea
  cuando hay varios scripts hablando entre sí en la misma prueba. Ejemplo real aplicado: los 4
  scripts del ciclo de vulnerabilidad del Atronach de Tormenta usan
  `[CAP_ThorMjolnir][Trigger]`, `[CAP_ThorMjolnir][Activator]`, `[CAP_ThorMjolnir][Vulnerability]`,
  `[CAP_ThorMjolnir][SpellDetector]`.
- `Debug.Notification` (el popup en pantalla) sigue siendo válido para avisos pensados para el
  jugador final en la versión terminada (p. ej. "Has quedado momentáneamente expuesto") — pero no
  como mecanismo de depuración por defecto en un script todavía en pruebas, una vez el proyecto ya
  tiene el logging activado como aquí.

## Permiso para editar, compilar y desplegar — SIEMPRE pedir permiso expreso antes de tocar un archivo del usuario

Añadido 2026-09-21 a petición del usuario y **ampliado el mismo día**. Contexto: al arreglar scripts de
uno en uno se editó el `.psc`, se compiló y se **desplegó el `.pex` directamente en el mod** sin
preguntar. El usuario pidió dejarlo documentado con la condición de "siempre siempre pide permiso", y
después precisó: **editar cualquier archivo suyo requiere permiso expreso, aunque el modo automático
lo deje pasar, no solo compilar.**

**Regla (sin excepciones ni permiso permanente):**
- **Antes de editar, crear, borrar o sobrescribir cualquier archivo del usuario** (`.psc`, `.pex`,
  `.esp`, documentos del repo como `QUEST-DESIGN.md`, `CHANGELOG.md` o `CLAUDE.md`, ficheros de esta
  skill, `.ini`, etc.), y **antes de compilar** (también a una carpeta temporal), pedir permiso
  **expreso en el chat y esperar la respuesta**. Que el modo automático ("auto-accept") permita la
  acción **no cuenta como permiso**: la pregunta la hago yo.
- **Una petición general no equivale a permiso.** "Corrígelo", "hagamos el punto 2" o "actualiza la
  doc" autorizan a *proponer*; antes de tocar nada hay que decir qué archivos y qué cambio, y esperar
  el sí.
- **La pregunta nombra cada fase que escribe algo**; puede ir en un solo mensaje, pero cada fase debe
  estar nombrada y aceptada (el usuario puede responder "solo 1 y 2"). Fases habituales:
  (1) editar `X.psc` (con el cambio resumido); (2) compilar para verificar; (3) desplegar el `.pex` en
  `mods\ThorMjolnir_OAR\Scripts\` (con la copia del anterior en el scratchpad); (4) actualizar
  `QUEST-DESIGN.md` u otro documento. Un sí a unas fases no cubre las demás.
- **No requiere permiso**: leer archivos, buscar y ejecutar comandos de solo lectura. Los ficheros
  temporales que necesite una compilación autorizada en el scratchpad de la sesión forman parte de esa
  petición.
- *Interpretación mía*: las notas de memoria de Claude (`.claude\projects\...\memory\`) las escribe
  Claude como parte de su funcionamiento; se avisa siempre que se escriban. Si el usuario quiere que
  también pidan permiso, cambiarlo aquí.

**Procedimiento, una vez concedido el permiso de cada fase:**
1. **Editar el `.psc` conservando saltos de línea y codificación.** Los `.psc` del proyecto son
   **CRLF y ANSI (CP1252, no UTF-8)**; comprobado que 3 tienen bytes no ASCII
   (`LightingDashAnimation`, con una `ö` en un texto para el jugador, `JormungandrStay` y `Mural01`).
   Reescribirlos en UTF-8 estropearía esas cadenas (⚠️ inferencia, no probado). **Verificado
   2026-09-21**: el Edit tool **conserva los CRLF** (inserciones y borrados de varias líneas en
   `AtronachVulnerability.psc`: 0 LF sueltos después). El **ANSI** (bytes no ASCII) sigue sin
   probarse con él: en los 3 ficheros con acentos usar Python (`cp1252`, saltos `\r\n`) o probar
   antes con una copia. Comprobar siempre después: sin LF sueltos y con los mismos bytes no ASCII.
2. **Compilar primero al scratchpad con `-keepasm`** y leer el bytecode de lo cambiado. Comando
   exacto y trampas de PowerShell en `references/papyrus-anti-patterns-fireundubh.md`, sección
   "Cómo repetir una prueba así". Import = `mods\ThorMjolnir_OAR\Scripts\Source` + el
   `Data\Scripts\Source` vanilla.
3. **Copia de seguridad** del `.pex` actual al scratchpad (`pex_backup\<nombre>.pex.orig`).
4. **Compilar con `-o=D:\Modlists\SME\mods\ThorMjolnir_OAR\Scripts`** (sin `-keepasm`), comprobar
   0 errores / 0 avisos y decir tamaño y hora del `.pex` nuevo.
5. **Actualizar `QUEST-DESIGN.md`** (regla de `CLAUDE.md`: script modificado) y dar al usuario el
   procedimiento de prueba, con el log a mirar. Para la prueba, arrancar el juego de nuevo.

**Datos verificados 2026-09-21:**
- El compilador es `D:\Steam\steamapps\common\Skyrim Special Edition\Papyrus Compiler\PapyrusCompiler.exe`
  (el mismo que usa la CK) con `TESV_Papyrus_Flags.flg`. No se sabe con qué opciones exactas lo lanza
  la CK; el resultado funcional es equivalente. **No hace falta compilar en la CK.**
- Los scripts viven en `mods\ThorMjolnir_OAR\Scripts\Source` (y los `.pex` en `...\Scripts`);
  `overwrite\Scripts` ya no existe (el usuario los movió al mod).
- Un cambio sin properties nuevas no obliga a tocar el `.esp` ni la CK. Una property de **valor** con
  valor por defecto en el `.psc` (`Float Property X = 3.0 Auto`) no hace falta rellenarla:
  `PollInterval` y `VulnerablePhaseTimeout` no están en el `.esp` (✅ volcado 2026-09-21) y el script
  funciona con su valor por defecto; ⚠️ `ImmuneNoticeCooldown`, añadida el mismo día, lo confirmará en
  la prueba. Una property de tipo **objeto** (Quest, Actor, Form…) queda en `None` si no se rellena en
  la CK. Renombrar o cambiar de tipo una property existente no se ha probado.
- Si el usuario recompila después ese script desde la CK, usará el `.psc` ya actualizado: no se
  pierde el cambio.
- **Preferencia del usuario (2026-09-21): compila él en la CK** («más seguro y fácil»). Por defecto
  Claude **edita el `.psc` y no compila a `Scripts\` ni despliega ningún `.pex`**; los pasos 3 y 4 del
  procedimiento solo se hacen si el usuario lo pide expresamente esa vez. Sí se compila al scratchpad
  para verificar (con permiso) y se le dice qué compilar y cómo probarlo.
- **El clasificador del modo automático puede bloquear escrituras** (2026-09-21): rechazó tres veces
  sobrescribir un `.psc` del mod ("Irreversible Local Destruction") con Python in situ, con
  `Copy-Item` de la versión completa y con un `Edit` que borraba un bloque, aun con copia de seguridad
  verificada por hash y permiso del usuario. Tras el "tú edita la fuente" explícito del usuario, los
  mismos cambios hechos como **ediciones puntuales con el Edit tool** sí pasaron (no se sabe si
  influyó ese mensaje o el orden de las ediciones). Ante un bloqueo así: no insistir con variantes,
  parar, explicar y esperar la decisión del usuario.
- Los fragmentos `QF_*` y `TIF_*` los genera la CK (zona *"Do not edit anything between this and the
  end comment"*): no editarlos a mano sin avisar; pedir al usuario que los cambie desde la CK.

## Precedente real de Bethesda a estudiar: "Lost to the Ages" (Dawnguard)

Esta es, de todo lo que se ha podido verificar, **la quest oficial más parecida en estructura** a
lo que el usuario describe: recolectar varios materiales de crafteo (los **Aetherium Shards**)
repartidos entre varias ruinas dwemer distintas, guiado por un NPC (el fantasma de Katria), para
fabricar un objeto legendario en una forja especial (la **Aetherium Forge**) y elegir una
recompensa final entre varias opciones. Detalle completo y citado en
`references/materiales-mundo-abierto.md`, sección "Aetherium Shard".

**Importante**: lo que hay verificado de esta quest es su walkthrough narrativo (qué hace el
jugador, en qué orden, en qué sitios) — no su árbol interno de Aliases/Stages, que UESP no
documenta. Si en algún momento hace falta ese nivel de detalle (por ejemplo, para replicar
exactamente cómo Bethesda estructuró el "elige tu recompensa" al final), la única forma fiable de
saberlo es abrir el registro `DLC1LD` directamente en la Creation Kit (con Dawnguard.esm cargado) o
en xEdit/SSEEdit — no asumirlo a partir del walkthrough.

## Materiales reales de Skyrim, verificados, con ubicación

Tabla completa y citada en `references/materiales-mundo-abierto.md`. Resumen rápido, todos
verificados contra `en.uesp.net`:

| Material | Dónde (verificado) | Nota |
|---|---|---|
| Ebony Ore/Ingot | Gloombound Mine (Narzulbur, Eastmarch); Raven Rock Mine (Solstheim, DLC Dragonborn); Blackreach; Redbelly Mine (The Rift); otras | Varias holds distintas de Skyrim continental + Solstheim |
| Stalhrim | Solo Solstheim (DLC Dragonborn) — 19 depósitos, ninguno en Skyrim continental | Requiere perk Ebony Smithing + quest "A New Source of Stalhrim" + pico nórdico antiguo |
| Daedra Heart | Cae de Dremora (no ligado a un lugar fijo) | Ingrediente de alquimia + crafteo daédrico, "Rare" |
| Aetherium Shard | Ruinas dwemer específicas (Arkngthamz y otras) | Objeto central de la quest oficial "Lost to the Ages" |
| Void Salts | Caen de Storm Atronachs (no ligado a un lugar fijo) | Solo confirmado como ingrediente de Alquimia, no de Smithing |

## Propuestas de diseño — esto es opinión mía, NO viene de ninguna fuente verificada

Todo lo de esta sección es una sugerencia, no un hecho — dejo esto separado a propósito para que
quede claro qué es dato verificado (arriba) y qué es idea mía sin respaldo:

- Dado que el arma de este proyecto tiene un fuerte tema nórdico/de tormenta (ver `Mecanica del
  arma.txt` y el nombre de trabajo "ThorMjolnir" ya usado en el propio código), **Ebony +
  Stalhrim** encajarían temáticamente sin inventar nada (son materiales reales, de zonas
  distintas — Skyrim continental y Solstheim) para un arma pesada con resistencia al frío. Añadir
  un tercer material ligado a un enemigo en vez de a un lugar (Daedra Heart, o Void Salts si se
  quiere tema de rayo) daría variedad de tipos de objetivo (minar vs. matar un tipo de criatura
  concreto) — pero esto es una combinación que me estoy inventando yo, no replica ninguna receta
  vanilla existente.
- Un esqueleto de stages posible, seguiendo la convención real de incrementos de 10 (esto es solo
  un ejemplo de numeración, no una plantilla obligatoria): `10` quest iniciada/encargo aceptado,
  `20`/`30`/`40` un stage por cada material encontrado (uno por objective), `100` todos los
  materiales reunidos → objective de volver a la forja, `200` arma fabricada / quest completa. De
  nuevo, esto es una propuesta mía razonable a partir de la convención documentada, no algo que
  haya visto en una quest real con este número exacto de stages.
- Si se quiere un momento de "fabricar el arma" con más peso narrativo que simplemente usar la
  mesa de trabajo, una Scene corta (varios NPCs reaccionando, o un forjador especial) encajaría con
  el mecanismo de Scenes ya documentado arriba — pero de nuevo, la Scene en sí (qué actores, qué
  diálogo) es una decisión de contenido del usuario, no algo que esta skill pueda proponer con
  ningún respaldo.

Si el usuario pide profundizar en cualquiera de estos puntos (un material concreto, una quest
vanilla concreta como precedente, un tipo de alias específico), usa la jerarquía de fuentes de
arriba antes de dar un dato como cierto — y si no se encuentra en ninguna, dilo así de claro en vez
de rellenar el hueco.

## Progreso concreto de la quest en curso (`CAP_ThorMjolnir_Quest_01`)

Esto ya no es guía genérica de la skill — es el estado real de esta quest en concreto, para no
perderlo entre sesiones. Actualizar aquí cada vez que se cierre un hito.

- **Hito 1 (altar → arranca la quest)**: hecho. Trigger Box (Activator invisible, `Is Marker` +
  `Ignored By Sandbox` marcados, `Obstacle` sin marcar) con el script vanilla
  `defaultSetStageTrigSCRIPT` (variante player-only, propiedades `myQuest`/`stage`/`doOnce`/
  `disableWhenDone`/`prereqStageOPT`) → `SetStage(10)`.
- **Hito 2 (leer/examinar un objeto → notificación)**: hecho. Activator `Mural`
  (`CAP_ThorMjolnir_Activator_Mural`, modelo reutilizado de `FalmerRosettaStone`), script propio
  `CAP_ThorMjolnir_Script_Mural01` (`OnActivate`, `PlayerREF` Auto-Fill en vez de
  `Game.GetPlayer()`) que muestra un `Message` form (`CAP_ThorMjolnir_MuralMessage`, popup con
  botón — no `Debug.Notification`, el texto es demasiado largo) y llama `SetStage(20)`. Alias
  `MuralAlias` (Specific Reference). Objective `10` → target `MuralAlias`. Stage `10` muestra el
  objective (`SetObjectiveDisplayed(10)`), Stage `20` lo completa
  (`SetObjectiveCompleted(10)`). Texto del mural: la leyenda del arma forjada por los hermanos
  enanos **Sindri y Brokk**, que reclama tres materiales.
- **Hito 3 (3 materiales) — en curso**:
  - **Material 1, mineral submarino — localización de referencia ya explorada y anotada
    (2026-09-01)**: celda **`(-24, 40)`** en el worldspace **`Tamriel`**, cerca de **Dawnstar**
    (costa norte, Mar de los Espíritus/Sea of Ghosts — pueblo de pesca y minería, buen gancho
    narrativo). Para volver: `cow Tamriel -24 40`. Nombre real del mineral/mena todavía sin
    decidir — pendiente de elegir uno real de Skyrim (ver tabla de materiales verificados más
    arriba) o confirmar que se inventa uno nuevo.
  - Materiales 2 y 3: sin decidir.
  - **Calcelmo (NPC de Markarth)**: guardado para más adelante como NPC *opcional* que da
    localización más exacta a jugadores que exploren menos — no bloqueante para el hito 3.
  - **Escena de ambientación resuelta (2026-09-09)**: activator `CAP_ThorMjolnir_Activator_Jormungandr`
    (criatura decorativa, sin package/AI) que se acerca al jugador al entrar en un trigger y se
    queda quieta en un `XMarkerHeading` — mecanismo completo (movimiento, sonido, aparición
    diferida con `Initially Disabled`+`Enable()`) verificado y documentado en
    `references/quest-mechanics-ck-wiki.md` tras un debugging largo (causa real del "no se mueve":
    la referencia estaba dentro de una Border Region).
  - **Patrón de recogida de material decidido**: Activator + script propio `OnActivate` →
    `PlayerRef.AddItem(ItemToGive, 1)` + `Self.Disable()` (+ sonido opcional vía Sound Marker) —
    no existe ningún script vanilla que combine "desaparecer + dar objeto" en una sola pieza
    (buscado en `Default_Scripts_List`, no encontrado), así que se usa uno propio. Detalle completo
    en `references/quest-mechanics-ck-wiki.md` → "Reproducir un sonido desde Papyrus al entrar en
    un trigger" (mismas trampas de Sound Marker/Output Model aplican aquí si se le añade sonido).
