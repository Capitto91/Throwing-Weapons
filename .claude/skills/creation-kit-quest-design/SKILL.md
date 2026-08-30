---
name: creation-kit-quest-design
description: Ayuda a planificar e implementar una quest de Skyrim SE en la Creation Kit (Quest Data, Aliases, Stages, Objectives, scripting Papyrus, Packages, Scenes, Constructible Objects) sin inventar nombres de campo/checkbox ni comportamiento no documentado. Pensada en particular para una quest de recolectar materiales en distintas partes del mundo para poder fabricar/desbloquear un arma, pero aplica a cualquier quest. Úsala siempre que el usuario pida diseñar, planificar o implementar una quest, un fetch quest, un sistema de recolección de materiales, o cualquier trabajo dentro de la Creation Kit de Skyrim (aliases, quest stages, dialogue, scenes, COBJ/constructible objects), incluso si no lo pide con la palabra "skill".
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
   reutilizable.
3. **`en.uesp.net`** (UESP general — objetos, ubicaciones, quests vanilla ya publicadas, lore). A
   diferencia de `ck.uesp.net`, **este dominio SÍ es fetchable directo** (confirmado con `curl`
   normal, sin necesitar user-agent especial ni nada — devuelve `200`). Es la fuente fiable para
   cualquier dato concreto de materiales/ubicaciones del juego real (qué mena hay en qué mina, qué
   ingrediente suelta qué criatura, qué hace una quest vanilla paso a paso). Ya hay un extracto
   compilado en `references/materiales-mundo-abierto.md` (Ebony, Stalhrim, Daedra Heart, Aetherium
   Shard, Void Salts) — para un material que no esté ahí, descárgalo con `curl` a un fichero
   temporal (ver método abajo) en vez de fiarte de memoria.
4. **Mirrors de formato de registro en GitHub** (`github.com/TES5Edit/meta`, que replica la
   documentación de formato de fichero de UESP usada por los devs de xEdit) — **fetchable directo
   con `WebFetch`**, sin el bloqueo de `ck.uesp.net`. Útil para confirmar la estructura interna
   exacta de un registro (p. ej. `QUSTDef.wiki` para el registro `QUST`) cuando hace falta más
   precisión de la que da la UI del editor, o como segunda fuente que contraste lo que dice el
   wiki de la CK.
5. **El propio registro real, abierto en la Creation Kit o en xEdit/SSEEdit** — la fuente más
   fiable de todas cuando está disponible: si el usuario tiene el juego instalado con sus DLCs, la
   forma de confirmar con certeza absoluta cómo Bethesda construyó una quest concreta (qué
   aliases, qué stages, qué conditions) es abrir ese registro directamente, no asumirlo desde un
   walkthrough de UESP (que documenta la experiencia del jugador, no el árbol de aliases interno).
   Ver más abajo, sección "Lost to the Ages", para un caso concreto donde esto aplica.
6. **Nunca** des por buena de memoria de entrenamiento el nombre exacto de un campo/checkbox de la
   CK, un tipo de alias, una convención numérica, o el nombre/ubicación de un objeto real del
   juego, sin haber pasado por 1-5. Si ninguna aplica, dilo.
7. **Ojo con mezclar ediciones del motor**: `ck.uesp.net` aloja tutoriales tanto del CK de Skyrim
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
