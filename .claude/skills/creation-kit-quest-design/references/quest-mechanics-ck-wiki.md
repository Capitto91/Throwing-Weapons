Extracto cacheado (2026-08-29) de hechos concretos sobre la Creation Kit de Skyrim, recopilados de
**ck.uesp.net** (antes creationkit.com/wiki, el wiki oficial de la Creation Kit).

**Importante sobre cómo se obtuvo esto**: `ck.uesp.net` está protegido por Cloudflare con un
challenge JS activo — confirmado en esta sesión que tanto `WebFetch` como `curl` (con
user-agent de navegador completo) reciben `HTTP 403 Forbidden` / `Cf-Mitigated: challenge` en
cualquier página de ese dominio, sin excepción. No es un problema puntual de una página (ya
documentado antes en `nif-vfx-practices/SKILL.md` para la página de `MagicEffect_Script`) — es un
bloqueo de todo el dominio para tráfico no-navegador. Todo lo de este archivo viene por tanto de
**snippets de resultados de `WebSearch`**, no de la página completa. Si necesitas un dato que no
está aquí, primero prueba `WebSearch` con el nombre exacto de la página (`ck.uesp.net "Nombre de
la Página"`) — si el snippet no basta, dile al usuario que abra la URL él mismo en su navegador y
pegue el contenido, o decide que el dato no está verificado.

## Estructura general de una Quest

Fuente: `https://ck.uesp.net/wiki/CreationKit:Quests`, `https://ck.uesp.net/wiki/Quest_Data_Tab`.

El formulario Quest se organiza en pestañas — Data, Objectives, Aliases, Dialogue Views, Papyrus
Fragments/scripts, Scenes (esta última es una lista de Scene forms asociados a la quest, no una
pestaña con contenido propio). Cada pestaña se documenta como página aparte en el wiki.

## Quest Data Tab

Fuente: `https://ck.uesp.net/wiki/Quest_Data_Tab`.

- **Priority**: determina qué diálogo/alias "gana" cuando dos quests compiten por el mismo actor o
  tipo de topic — dialogue y aliases de mayor prioridad tienen precedencia sobre los de una quest
  de prioridad menor cuando hay solape.
- **Quest Type**: determina el icono que se muestra junto a la quest en la lista del jugador; las
  quests marcadas como "Miscellaneous" aparecen en esa categoría (solo se muestran sus
  objectives).
- **Start Game Enabled**: solo disponible si la quest no tiene un Event que la dispare. Si está
  marcado, la quest empieza a correr al inicio de la partida. Si no, no arranca hasta que algo
  llame a `Start`, `SetStage` o `SetObjectiveDisplayed` sobre ella.

## Cómo crear el formulario Quest (navegación del Object Window)

Fuente: `WebSearch` sobre `ck.uesp.net "CreationKit:Quests"` (snippet, mismo bloqueo de Cloudflare
del resto del archivo).

- En el **Object Window**, categoría **Character → Quest**: clic derecho en cualquier punto de la
  lista de quests existentes → **New**. Esto abre la ventana de la quest ya en su pestaña **Data**
  por defecto.
- Las cuatro partes principales de una quest, citadas literalmente: **Aliases, Stages, Objectives
  y Dialogue** (topics y scenes).

## Campos reales del subrecord `DNAM` (Quest Data) — confirmado contra el formato binario

Fuente: `https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/QUST` (fetchable directo con `curl`,
sin el bloqueo de `ck.uesp.net` — descargado y leído completo en esta sesión, no es snippet).
Esto es el registro real tal como lo guarda el motor, así que es la fuente más precisa posible
para lo que hace cada campo de la pestaña Data — más fiable incluso que un tutorial, porque no
depende de que el autor del tutorial haya descrito bien la UI.

- **Flags (byte 1)**: `0x01` = **Start Game Enabled**; `0x02` sin usar; `0x04` = *Wilderness
  Encounter* (con interrogante en la propia fuente, no 100% confirmado su efecto exacto); `0x08` =
  **Allow repeated stages**; `0x10` usado internamente pero no se muestra en la UI de la CK.
- **Flags (byte 2)**: `0x01` = **Run Once**; `0x02` = **Exclude from dialogue export**; `0x04` =
  **Warn on alias fill failure**; `0x08` sin usar; `0x10` usado internamente pero no se muestra en
  la UI.
- **Priority**: `uint8`, valor de **0 a 100**.
- **Quest Type** (`uint32`), lista completa de valores posibles:
  `0` = None (la quest no aparece en el diario), `1` = Main Quest, `2` = Mages' Guild, `3` =
  Thieves' Guild, `4` = Dark Brotherhood, `5` = Companion Quests, `6` = **Miscellaneous** (aparece
  en la sección Miscellaneous del diario; el nombre de la quest se oculta y solo se muestran sus
  objectives — confirma lo ya citado arriba desde `Quest_Data_Tab`), `7` = Daedric Quests, `8` =
  Side Quests, `9` = Civil War, `10` = DLC01 - Vampire (Dawnguard), `11` = DLC02 - Dragonborn.
- **ENAM (Event)**: `char[4]`, corresponde al nombre corto del subrecord `SMEN` — es el campo real
  detrás del selector "Event" de la pestaña Data (el que, al asignarse, deshabilita Start Game
  Enabled, ver más abajo).
- Confirma también, desde el lado binario, la separación entre **Quest Dialogue Conditions**
  (`CTDA` antes del marcador `NEXT`) y **Quest Event Conditions** (`CTDA` después, descritas
  literalmente como "SM event node conditions") — mismo concepto ya documentado en la sección de
  Story Manager más abajo, ahora confirmado que son dos bloques de condiciones distintos a nivel
  de registro.

## Checkboxes de la pestaña Data — confirmado visualmente contra un screenshot real de la CK (2026-08-31)

El usuario compartió un screenshot de su propia quest ya creada (`CAP_ThorMjolnir_Quest_01`, CK de
este proyecto) — permite confirmar de primera mano, no solo por wiki, el layout real de la pestaña
Data y su correspondencia exacta con los flags de `DNAM` documentados arriba:

- Fila de checkboxes bajo Priority/Event: **Start Game Enabled**, **Run Once**, **Warn on alias
  fill failure** (izquierda a derecha) — coincide 1:1 con el orden de bits del byte de flags 2
  documentado en `DNAM` (`0x01`/`0x02`.../`0x04`, salvo que Start Game Enabled es en realidad del
  primer byte — la UI los mezcla en una sola fila visualmente aunque sean bytes distintos del
  registro).
- Segunda fila: **Allow repeated stages** sola.
- Arriba a la derecha, junto a los botones de exportar: **Exclude from dialogue export**.
- Existe también un campo **Object Window Filter** (vacío por defecto) — corresponde al subrecord
  `FLTR` (`zstring`) documentado arriba, solo organiza el árbol del Object Window, sin efecto en
  el juego.

### `Run Once` — trampa confirmada, relevante para cualquier quest que se pueda necesitar resetear

Fuente: `WebSearch` sobre `ck.uesp.net "Quest Data Tab" "Run Once"` (snippet).

- *"Run Once" prevents the quest from being reset when it starts. If a "Start Game Enabled" quest
  is not also flagged to "Run Once", its OnInit event will fire twice.* — el segundo efecto solo
  aplica a quests con Start Game Enabled (no es el caso de una quest arrancada por Trigger Box).
- **El efecto que sí importa aquí**: *"all forms of resetting including Reset() will not work when
  this is checked"* — con Run Once marcado, ni `Quest.Reset()` desde Papyrus ni ningún otro
  mecanismo de reset de la CK funcionan sobre esa quest, nunca, mientras el flag siga activo.
- Implicación práctica para una quest de recolección con una posible reimplementación futura de
  save-integrity (ver `CLAUDE.md` del proyecto, sección de cosave pendiente): si alguna vez hace
  falta poder reiniciar esta quest (bug de un jugador, testing, o un futuro sistema de
  "abandonar y reintentar"), Run Once lo bloquea por completo. Marcarlo es una decisión consciente
  de "esta quest se vive una sola vez por partida, sin vuelta atrás" — no un checkbox neutro.

## Quest Stages Tab

Fuente: `https://ck.uesp.net/wiki/Quest_Stages_Tab`, `https://ck.uesp.net/wiki/GetStage`,
`https://ck.uesp.net/wiki/Bethesda_Tutorial_Planning_the_Quest`.

- Cada stage tiene un índice de `0` a `65535`.
- Cada stage puede tener 1 o más **"stage items"** — son los que llevan el result script
  (fragmento Papyrus) y/o el Log Entry. **Hace falta al menos un stage item en un stage para
  poder adjuntarle lógica de script o un log entry** (esto también lo confirma
  `Bethesda_Tutorial_Quest_Objectives`, ver más abajo).
- Marcar la casilla **Complete Quest** en un stage dispara el mensaje "Quest Completed" y mueve la
  quest de activa a completada en la lista del jugador. También existe una opción de **Fail
  Quest** para stages de fracaso.
- **Convención de numeración, confirmada como recomendación explícita del propio tutorial de
  Bethesda** (no solo costumbre de la comunidad): numerar los stages en incrementos de 10 (10, 20,
  30...) — da margen para insertar stages intermedios más adelante sin tener que renumerar todo lo
  existente.
- El tutorial oficial "Planning the Quest" plantea como ejemplo de referencia una quest lineal
  clásica (heredada de versiones anteriores del Construction Set): **Bendu Olo**, un Dunmer al que
  le han robado un amuleto, escondido en manos de un ladrón en una cueva cercana; ofrece pagar el
  doble de su valor si se recupera. El tutorial trocea esta quest en fases secuenciales del tipo
  "el jugador ha hablado con Bendu Olo y ha aceptado el encargo" → "el jugador ha matado al
  ladrón" → "el jugador ha recuperado el amuleto" → "el jugador ha devuelto el amuleto y cobrado la
  recompensa" — y recomienda pensar cada stage como "el evento más reciente que ha ocurrido", no
  como una acción en curso.
  - El tutorial también ilustra bifurcación de diálogo: una rama "No" puede fijar el stage a un
    valor (ejemplo citado: 5) y una rama "Sí" a otro (ejemplo citado: 10) — el guion de cada línea
    de diálogo llama a `SetStage` con el valor que corresponda a esa rama.
- `GetStage`/`SetStage` (funciones de `Quest` en Papyrus) son la forma estándar de leer/avanzar el
  progreso de una quest desde cualquier script.

## Quest Objectives Tab

Fuente: `https://ck.uesp.net/wiki/Quest_Objectives_Tab`,
`https://ck.uesp.net/wiki/Bethesda_Tutorial_Quest_Objectives`.

- Cada Objective tiene un índice (número), un **Display Text** (lo que ve el jugador en la lista
  de quests activas) y una lista de **targets** — cada target apunta obligatoriamente a una de las
  **Reference Aliases** de la propia quest, y puede llevar sus propias condiciones. Ese target es
  lo que coloca la flecha del compass/mapa sobre el objeto/actor/lugar correspondiente mientras el
  objective está activo.
- Confirmado (coincide con la nota de Quest Stages): para que un stage pueda mostrar/ocultar un
  objective o correr lógica de script, necesita al menos un "quest stage item" configurado.
- `SetObjectiveDisplayed(objectiveID, bDisplayed, bForce)` es la función de `Quest` en Papyrus para
  mostrar/ocultar un objective por código — el parámetro `bForce` fuerza a mostrarlo aunque ya se
  hubiera mostrado antes.

## Quest Alias Tab — el mecanismo relevante para "un material en cada sitio del mundo"

Fuente: `https://ck.uesp.net/wiki/Quest_Alias_Tab`. Contrastado también contra el formato interno
real del registro QUST (ver `QUSTDef.wiki` más abajo) — mismos conceptos, ahora con el nombre de
subrecord binario que usa cada uno.

- Una alias es un "rol"/etiqueta (actor, objeto o location) que la quest usa en scripts, packages
  y diálogo **en vez de** una referencia fija del mundo — permite que la quest decida en tiempo de
  ejecución qué referencia concreta cumple ese rol.
- Cuando la quest arranca, las aliases se rellenan **en orden** — el orden de la lista importa
  cuando una alias depende directa o indirectamente de otra (p. ej. una Location Alias que a su
  vez alimenta a otra alias que busca algo dentro de esa location).
- **Tipos de "fill" confirmados** (cómo se rellena una alias al arrancar la quest):
  - **Specific Reference**: se asigna una referencia concreta y fija del mundo a esta alias.
  - **Unique Actor**: se elige un actor único (un NPC con Editor ID propio) para rellenar la
    alias — **solo funciona si a la referencia de ese actor se le ha asignado un "Persist
    Location"** (si no, la alias no se rellena).
  - **Location Alias + Location Ref Type**: se elige otra alias de tipo Location ya definida más
    arriba en la lista (tiene que estar por encima en el orden) y un "Location Ref Type"; al
    arrancar, el Story Manager busca dentro de esa location una referencia que tenga ese loc ref
    type y la usa para rellenar esta alias.
  - **External Quest Alias**: se elige otra quest y una Reference Alias suya; al arrancar esta
    quest, la alias se rellena con lo que sea que tenga esa alias externa en ese momento.
  - **Create Reference (Created Object)**: al arrancar la quest se crea una referencia nueva a
    partir de un base object elegido, y esa referencia rellena la alias.
- **No verificado / no encontrado para el CK de Skyrim**: un tipo de alias de "colección" que
  agrupe varias referencias bajo una sola alias (algo así existe en el CK de **Fallout 4**,
  "Reference Collection Alias" — ver más abajo, "Fallout 4 vs Skyrim"). Para Skyrim, el patrón
  verificado para "necesito rastrear varios objetos distintos" es **una Reference Alias por cada
  objeto/material**, no una alias que contenga una lista.
- Las aliases pueden llevar Conditions propias (qué debe cumplirse para que un candidato concreto
  sirva para rellenarla), Package Data (comportamiento de IA si es un actor), Spells, Factions,
  Keywords e inventario inicial (`CNTO`) — y se les puede adjuntar un script Papyrus propio
  (`ReferenceAlias`/`ObjectReferenceAlias` según el tipo, extendiendo la clase base
  correspondiente) en vez de tener que escribir toda la lógica en el Quest script.

### Formato interno real del registro QUST (confirma lo anterior desde el otro lado, el binario)

Fuente: `https://github.com/TES5Edit/meta/blob/master/UESPWiki/QUSTDef.wiki` (mirror en GitHub de
la documentación de formato de archivo de UESP, usada por los devs de xEdit — **sí es fetchable
directo**, sin el bloqueo de Cloudflare de `ck.uesp.net`).

- Stages: subrecord `INDX` (número de stage + flags de startup/shutdown/keep instance), `QSDT`
  (flags de Complete Quest/Fail Quest), `CNAM` (texto del log entry), `CTDA` (condiciones del
  stage).
- Objectives: subrecord `QOBJ` (índice, por convención suele coincidir con el índice del stage que
  lo activa), `FNAM` (flags, incluye "ORed With Previous"), `NNAM` (texto del objective), `QSTA`
  (asignación de target alias + si lleva marcador de compass).
- Aliases: dos tipos de subrecord contenedor, `ALST` (Reference alias) y `ALLS` (Location alias).
  Fill type se codifica como uno de: `ALUA` (Unique Actor), `ALCO` (Created Object), `ALEQ`
  (External Quest alias), `ALFE` (event-based), `ALFL` (Forced Location, solo en `ALLS`), `ALFR`
  (Forced Reference, solo en `ALST`), `ALRT` (Location Ref Type lookup) — coincide exactamente con
  los cinco tipos de fill listados arriba desde la UI del editor. Subrecords adicionales por
  alias: `ALFC` (facciones), `ALPC` (package data), `ALSP` (hechizos), `CNTO` (inventario inicial),
  `KWDA` (keywords), `ALDN` (nombre a mostrar, referencia a un `MESG`), `VTCK` (voice types
  válidos).

## Scripting Papyrus del ciclo de la quest

Fuente: `https://ck.uesp.net/wiki/Bethesda_Tutorial_Basic_Quest_Scripting`,
`https://ck.uesp.net/wiki/Quest_Script`, `https://ck.uesp.net/wiki/Quest_Stage_Fragments`.

- El lenguaje de scripting de la Creation Kit es Papyrus — los "fragments" de un stage son scripts
  que extienden `Quest` y corren automáticamente cuando ese stage se activa (se editan desde la
  propia pestaña de Stages, sin crear un `.psc` a mano para cada uno).
- Patrón citado literalmente en el tutorial oficial: dentro de un fragment de stage,
  `SetObjectiveDisplayed(10)` para mostrar el objective de índice 10 en el momento en que ese
  stage se activa.
- Un Quest script "de verdad" (no un fragment) se escribe como cualquier otro script Papyrus que
  extiende `Quest`, y desde fuera de la quest se puede llamar a sus funciones (`SetStage`,
  `SetObjectiveDisplayed`, etc.) a través de una propiedad `Quest` apuntando a ella.

**Patrón de comunidad, NO texto literal de un tutorial oficial** (fuente: foros de Nexus Mods —
tratarlo como "patrón ampliamente usado", no como spec oficial de Bethesda): para detectar que el
jugador ha recogido/depositado un material concreto, es habitual condicionar con
`GetItemCount()` sobre la referencia que corresponda (el propio jugador vía `Game.GetPlayer()`, o
`(MiAlias.GetReference() as ObjectReference).GetItemCount(MiMaterial)` si el material vive en un
contenedor con su propia alias) y, cuando se cumple, llamar a `SetStage(...)` sobre la quest. Esto
se puede comprobar puntualmente en un evento (p. ej. `OnItemAdded` de un `ReferenceAlias`) o desde
el fragment de un stage de "comprobación" que vuelve a evaluarse — decide la forma exacta según lo
que ya sepas de Papyrus, esto no es una API con nombre fijo, es solo el patrón general.

## Packages y viaje a un lugar (relevante si un NPC guía al jugador, o si el material está
custodiado)

Fuente: `https://ck.uesp.net/wiki/Category:Packages`, `https://ck.uesp.net/wiki/AI_Packages_Tab`,
`https://ck.uesp.net/wiki/Category:Package_Templates`, `https://ck.uesp.net/wiki/Bethesda_Tutorial_Radiant_Quests`.

- Un Package puede marcarse como propiedad de una quest, y su Package Data puede apuntar a una
  **Quest Alias** en vez de a una referencia fija del mundo — ejemplo citado literalmente: el
  package de viaje de un NPC puede apuntar a un radio alrededor de la alias "WidgetOfDoom" de
  "MyQuest", en vez de a una localización fija.
- Un **Package Template** es la versión "molde" de un Package — define el comportamiento base
  (p. ej. "viajar a X") para que varios Packages concretos solo tengan que cambiar el destino,
  reutilizando la misma lógica de fondo.
- El tutorial oficial de **Radiant Quests** introduce el concepto de **"seed alias"**: la alias
  inicial de la que dependen las demás (en su ejemplo, el ladrón elegido determina también qué
  mazmorra y qué marcador de mapa se usan — tres aliases que dependen todas de esa elección
  inicial). Aplicable si la quest de recolección decide dinámicamente en qué orden/lugar aparece
  cada material, en vez de tenerlo todo fijo a mano.

## Scenes (opcional — útil para un momento de "forjar el arma" con varios actores coordinados)

Fuente: `https://ck.uesp.net/wiki/Bethesda_Tutorial_Scenes`, `https://ck.uesp.net/wiki/Category:Scenes`.

- Una Scene se puede lanzar marcando su casilla **"Begin on quest start"**, o disparándola por
  script.
- Están pensadas para coordinar a varios actores en una secuencia (diálogo, timers, packages y
  scripts corriendo en paralelo) — desde una conversación simple entre dos NPCs hasta una pieza de
  cinemática completa de una quest.

## Constructible Object (COBJ) — el paso final de "fabricar el arma"

Fuente: `https://ck.uesp.net/wiki/Constructible_Object`,
`https://en.uesp.net/wiki/Skyrim_Mod:Mod_File_Format/COBJ` (mismo registro, documentación de
formato — esta última **sí es fetchable directo**, está en `en.uesp.net` no en `ck.uesp.net`).

- Un COBJ describe una receta de crafteo — se usa tanto para cocina, fundición, forja, afiladora
  y mesa de trabajo, con la única diferencia real entre "crea un objeto nuevo" y "mejora uno
  existente" siendo el **Workbench Keyword** que llevan.
- Campos relevantes para exigir varios materiales a la vez: **Created Object** (el resultado —
  aquí iría el arma), **Workbench Keyword** (qué estación de trabajo lo habilita), **Category**,
  una lista de **Items** requeridos, cada uno con su **cantidad**, y **Conditions** — las
  condiciones son la vía estándar para que la receta solo aparezca disponible cuando corresponda
  (p. ej. `GetStage` de la quest de recolección en un valor concreto, o un `Global` puesto a 1 al
  completar la recolección) en vez de estar siempre visible en la mesa de trabajo.
- El flujo de trabajo habitual documentado (Nexus Mods Wiki, "Making an item craftable for
  Skyrim") es duplicar una receta ya existente parecida (p. ej. `RecipeWeaponIronSword`) como
  plantilla y editar sus campos, en vez de crear el registro desde cero.

## Arrancar una quest automáticamente al llegar a un sitio (sin Start Game Enabled, sin activar nada a mano)

Añadido 2026-08-31, via `WebSearch` (snippets, mismo bloqueo de Cloudflare que el resto del
archivo). Dos mecanismos verificados, distintos entre sí:

### Opción A — Trigger Box con script `OnTriggerEnter`

Fuente: `https://ck.uesp.net/wiki/OnTriggerEnter_-_ObjectReference`,
`https://ck.uesp.net/wiki/Default_Scripts_List`, foro Nexus Mods ("Quest trigger doesn't (always)
fire").

- Se coloca una **Trigger Box** (un activator de volumen invisible) en la celda del mundo abierto,
  en la posición del altar.
- El evento `OnTriggerEnter(ObjectReference akActionRef)` (de `ObjectReference` script, heredado
  por cualquier referencia con volumen de trigger) salta cuando algo entra en ese volumen —
  `akActionRef` es lo que ha entrado, hay que comprobar que es el jugador (`akActionRef ==
  Game.GetPlayer()`) antes de reaccionar, porque también salta con NPCs/criaturas.
- Existe un **script por defecto ya hecho en el editor** para este patrón exacto:
  `defaultSetStageTRIGSpecificActor` (listado en `Default_Scripts_List`) — pensado para poner una
  quest a un stage concreto cuando un actor específico entra en el trigger, sin escribir un script
  propio desde cero. Confirmado que existe en la lista de scripts por defecto; no confirmado en
  detalle sus propiedades exactas (probablemente una property `Quest` + un `int` de stage + el
  actor a comprobar) — abrir el script en el editor para ver sus properties exactas antes de
  usarlo.
- Nota de un hilo de foro (Nexus Mods, no oficial): un Trigger Box puede fallar de forma
  intermitente si el jugador entra en la zona muy rápido (fast travel, teletransporte) en vez de
  caminando — vale la pena tenerlo en cuenta si el altar es accesible por viaje rápido cercano.

### Opción B — Story Manager + "Change Location Event"

Fuente: `https://ck.uesp.net/wiki/Quest_Data_Tab`, `https://ck.uesp.net/wiki/Change_Location_Event`,
`https://ck.uesp.net/wiki/SM_Event_Node`.

- El **Story Manager** puede arrancar una quest en respuesta a eventos que el propio motor genera
  automáticamente (`SM Event Node`, categoría "Character" en el Object Window) — uno de esos
  eventos es **Change Location Event**, que salta cada vez que el jugador (u otro actor) entra en
  una **Location** nueva (no una celda cualquiera — una `Location` es un tipo de formulario propio
  de la CK que se asigna a una zona).
- `Change Location Event` trae **Actor** (quien cambió de location, casi siempre el jugador),
  **Old Location** y **New Location** como Event Data.
- Para que una quest arranque así: en la pestaña **Data** de la quest, el campo **Event** se pone a
  `Change Location Event` (u otro SM Event) — **en cuanto una quest tiene un Event asignado, la
  casilla Start Game Enabled queda deshabilitada/gris** (confirma y explica la nota ya existente
  arriba en "Quest Data Tab": son mutuamente excluyentes, una quest o arranca sola al inicio de
  partida, o arranca por un Event del Story Manager, no las dos formas a la vez).
  Fuente literal: *"Any quest which specifies an Event can only be started by the Story Manager
  (Start Game Enabled will be greyed out)."*
- Con el Event asignado, las Aliases de la quest pueden leer los datos de ese evento concreto (p.
  ej. rellenar una alias con la Location que disparó el evento) — no solo sirve para arrancar la
  quest, también para alimentarla con contexto.
- Para restringir el disparo a **la Location del altar específicamente** (no cualquier cambio de
  location del juego), hace falta una **Condition** en el nodo de evento/en la quest comprobando
  que `New Location` es la Location que se le haya asignado al altar (p. ej. `GetEventData ==
  <MiLocationDelAltar>`, patrón citado para el caso análogo de "entrar en una posada" comprobando
  el keyword `LocTypeInn`) — el propio altar tendría que tener asignada su propia `Location` en el
  editor de celdas para que esto funcione.

### Cuál usar — sin fuente que diga "mejor práctica", esto es criterio propio no verificado

No se ha encontrado ninguna fuente que declare una opción superior a la otra en general. Diferencia
práctica observada en las fuentes: el Trigger Box es más simple de montar (un objeto + un script) y
da control geométrico exacto (el volumen exacto del trigger); el Story Manager con Change Location
Event depende de que el altar tenga asignada una `Location` propia (un paso extra de setup) pero
integra mejor con el resto de la maquinaria de Radiant Story y no depende de un volumen físico que
haya que dimensionar a mano. Para "un altar puntual en el mundo abierto", el Trigger Box (Opción A)
parece más directo — pero esto es una recomendación mía, no un hecho citado.

## Activator invisible como base de un Trigger Box — flags verificados

Añadido 2026-08-31. Patrón usado para el trigger del altar de la quest en curso: un `Activator`
propio (Object Window → WorldObjects → Activator → New) **sin `Model` asignado**, que luego se
coloca en el mundo y recibe una `Primitive` (el volumen de trigger en sí, ver arriba "Creating
Primitives") sobre su referencia. Fuente de los flags:
`https://ck.uesp.net/wiki/Activator` (snippets de `WebSearch`, mismo bloqueo de Cloudflare del
resto del archivo).

- **Is Marker**: *"Object is treated as a marker and won't be rendered in game"* — es el flag que
  hace que el activator sea invisible de verdad (junto con no asignarle `Model`). Recomendado para
  cualquier activator que solo sirva de base a un volumen de trigger, sin representación visual
  propia.
- **Obstacle**: *"Dynamically cuts the navmesh in game, preventing actors from walking into
  it (...) only works with NIFs that have the correct collision layer set"* — inoperante sin
  `Model` (no hay NIF), pero además hay que dejarlo **sin marcar** para un trigger, porque el
  objetivo es justo que se pueda caminar a través de él.
- **Ignored By Sandbox**: *"Actors on sandbox packages will ignore this object"* — recomendado
  marcarlo en cualquier activator invisible, para que ningún NPC con IA de sandbox intente
  "usarlo" como si fuera mobiliario/idle marker.
- **On Local Map**: *"Determines whether an object's local map representation, if any, will
  appear on the local map. Checked by default"* — sin `Model` no hay representación que mostrar,
  así que es indiferente que quede marcado o no.
- El resto de checkboxes de la ventana del Activator (Dangerous, Has Tree LOD, Ignore Object
  Interaction, Has Platform/Language Specific Textures, Random Anim Start, Child Can Use, Must
  Update Anims) actúan sobre geometría/textura/animación de un `Model` — no verificados uno a uno
  en detalle porque son inoperantes sin malla asignada, pero no hay ninguna razón para activarlos
  en este patrón.
- **NavMesh Generation Import Option** y **Water Type** también dependen de que haya geometría
  real que importar/desplazar — sin `Model`, cualquier valor es equivalente.
- **Default Primitive Color**: solo el color con el que el editor dibuja el volumen en el
  viewport — ayuda visual pura, sin efecto en el juego.

## Mover un objeto de un punto A a un punto B por script — Activator + XMarker + TranslateToRef

Añadido 2026-09-09, para el caso de un activator de ambientación (modelo de criatura, sin
package/AI) que se acerca al jugador al entrar en un trigger y se queda quieto en el destino.

### Firmas exactas (fuente: `papyrus.bellcube.dev`, fetchable directo, cita el CK Wiki)

- `https://papyrus.bellcube.dev/skyrimse/script/objectreference/function/translateto/`:
  `TranslateTo(float afX, float afY, float afZ, float afXAngle, float afYAngle, float afZAngle,
  float afSpeed, float afMaxRotationSpeed=0.0)` — traslada a coordenadas/ángulos absolutos.
  `afMaxRotationSpeed=0.0` significa que la rotación destino se alcanza a la vez que la posición;
  no es posible rotar más rápido que eso. Dispara `OnTranslationComplete` cuando posición y
  rotación coinciden; `OnTranslationAlmostComplete` puede saltar aunque la rotación aún esté lejos.
- `https://papyrus.bellcube.dev/skyrimse/script/objectreference/function/translatetoref/`:
  `TranslateToRef(ObjectReference arTarget, float afSpeed, float afMaxRotationSpeed=0.0)` — mismo
  mecanismo que `TranslateTo` pero igualando posición/rotación a otra referencia en vez de a
  coordenadas fijas. Valores negativos de velocidad citados como causa de "rotación
  impredecible".

### Qué tipo de objeto puede moverse así — Activator sí, Static no (sin convertir)

Fuente: `ck.uesp.net` ("TranslateTo - ObjectReference", "ObjectReference Script"), vía snippets de
`WebSearch` (mismo bloqueo de Cloudflare de siempre).

- Citado literalmente: para mover algo que no sea el jugador por script, la property debe ser un
  `ObjectReference` apuntando a un actor o un activator — **"only activators and actors work, not
  statics"**.
- Un `Static` normal no mueve su caja de colisión al trasladarlo — hace falta declararlo
  `MovableStatic` y corregir el `bhkRigidBody` de su malla para que la colisión siga a la
  malla. Para un activator de ambientación sin necesidad de colisión física fina, esto hace del
  `Activator` la opción más simple frente a `Static`/`MovableStatic`.
- **Trampa citada, a vigilar en el juego**: *"Using havok collision in an activator then trying to
  translate it doesn't work—it just sits there motionless"* — si la malla del activator lleva un
  `bhkRigidBody` activo, `TranslateTo`/`TranslateToRef` puede no moverlo en absoluto. **Solo
  aplica si el NIF tiene de verdad una malla de colisión** — confirmar esto primero (abrir el NIF y
  comprobar si hay algún bloque `bhk*`) antes de gastar tiempo en el remedio de abajo.
  - Remedio documentado para cuando sí aplica: `ObjectReference.SetMotionType(int aeMotionType,
    bool abAllowActivate=true)` (fuente:
    `papyrus.bellcube.dev/skyrimse/script/objectreference/function/setmotiontype/`, cita el CK
    Wiki) — `Motion_Keyframed=4` se describe literalmente como *"the object will NOT be simulated
    by Havok"*; llamarlo una vez antes de `TranslateTo`/`TranslateToRef` saca la malla del control
    de Havok. Mismo concepto que ya usa este proyecto en C++ para el SKSE plugin (`kKeyframed`, ver
    `CLAUDE.md` → "Arquitectura de física de proyectiles"). Tabla completa de constantes:
    `Motion_Dynamic=1`, `Motion_SphereInertia=2`, `Motion_BoxInertia=3`, `Motion_Keyframed=4`,
    `Motion_Fixed=5`, `Motion_ThinBoxInertia=6`, `Motion_Character=7`.
- **Caso concreto resuelto (2026-09-09, activator `CAP_ThorMjolnir_Activator_Jormungandr`, "no se
  mueve" al entrar en el trigger) — CAUSA REAL CONFIRMADA EN EL JUEGO**: descartado que el NIF
  tenga malla de colisión (no la tiene) o relación con IceWraith/retargeting de criatura (no la
  tiene) — es un Activator simple con una animación horneada que es un idle estático, movido por
  script vía `TranslateToRef`. Se verificó exhaustivamente todo el lado Papyrus sin encontrar nada
  mal: `OnTriggerEnter` se disparaba, la comparación con `PlayerRef` pasaba, `CreatureRef` y
  `TargetMarker` no eran `None` y apuntaban a los FormID correctos (confirmado con
  `Debug.MessageBox(prop as String)`), `TranslateToRef` se ejecutaba sin ningún error en el log, e
  incluso comparando `GetPositionX/Y/Z()` antes y después con 10s de espera a 2000 unidades/s la
  posición **no cambiaba en absoluto**.
  - **Causa real**: el activator estaba colocado dentro de una **Border Region** (región de borde
    del worldspace exterior de Tamriel). Confirmado por el usuario: moviendo la misma referencia a
    otra ubicación fuera de esa Border Region, `TranslateToRef` funciona con normalidad
    inmediatamente, sin ningún otro cambio.
  - **Implicación práctica**: si una referencia colocada en el mundo exterior "no se mueve" con
    `TranslateTo`/`TranslateToRef` pese a que todo el código/properties están verificados uno a uno
    sin fallos, **comprobar primero si esa referencia cae dentro de una Border Region** (visible en
    el render window como una región especial de borde de mapa) antes de seguir depurando el
    script — es la explicación más simple y la que realmente aplicó aquí, y no está documentada en
    ninguna fuente externa (`ck.uesp.net`/foros) consultada durante este debugging; es hallazgo
    propio de esta sesión, verificado en el juego.
  - **Sin workaround conocido para dejarlo dentro de la Border Region**: se buscó explícitamente
    (`ck.uesp.net`, foros de Nexus Mods) una forma de que la referencia se mueva igualmente estando
    dentro de una Border Region, sin encontrar nada — el CK Wiki solo documenta qué es una Border
    Region (marca el límite jugable del worldspace; fuera de ella hay un muro invisible), no el
    mecanismo interno exacto por el que interfiere con `TranslateTo`, ni ninguna forma de excluir
    una referencia concreta de su efecto. Existe un mod ("Skyrim Borders Disabled") que desactiva
    las fronteras del mapa, pero es un cambio global de worldspace, no algo aplicable a un objeto
    suelto — no recomendado solo para este caso. **La única solución confirmada es reubicar la
    referencia fuera de la Border Region** (verificado por el usuario: la misma referencia, movida
    a otra ubicación, funciona de inmediato sin ningún otro cambio).

## Reproducir un sonido desde Papyrus al entrar en un trigger

Añadido 2026-09-09, mismo hilo de debugging del activator Jormungandr.

- **Mecanismo real**: no existe ningún `PlaySound` en `ObjectReference` (confirmado, no aparece en
  su lista de funciones en `papyrus.bellcube.dev`). La función real es
  `Sound.Play(ObjectReference akSource)` (fuente:
  `papyrus.bellcube.dev/skyrimse/script/sound/function/play/`, cita el CK Wiki) — *"Play this
  sound base object from the specified source"*. Se necesita una property de tipo `Sound Property
  ApproachSound Auto`.
- **Trampa confirmada al rellenar esa property en el editor**: el selector de una property `Sound`
  en la ventana de Properties de la CK **no lista Sound Descriptors (`SNDR`) directamente** — pide
  un **Sound Marker** (fuente: foro `gamesas.com`, vía `WebSearch` — *"the Sound property wants
  Sound Markers, not Sound Descriptors — you'll either need to create one that references the
  Sound Descriptor, or use an existing Sound Marker"*). Flujo correcto: crear un `Sound Marker`
  (`Object Window → Audio → Sound Marker`) que referencie el `Sound Descriptor` real que quieres
  reproducir, y seleccionar ese Sound Marker (no el Descriptor) como valor de la property.
- **Trampa confirmada — "suena en el preview de la CK pero no en el juego"**: un `Sound Descriptor`
  **necesita tener asignado un Output Model** para poder oírse en tiempo de juego real (fuente:
  `ck.uesp.net/wiki/Sound_Output_Model`, vía `WebSearch` — *"sounds always tie to a Sound Output
  Model"*). El botón de preview de la CK reproduce el archivo de audio en bruto sin pasar por el
  motor de audio del juego, así que sí suena ahí aunque falte el Output Model — en el juego, sin
  Output Model, `Sound.Play()` no da ningún error pero no se oye nada. Revisar ese campo en el
  propio Sound Descriptor si el sonido "no suena" pese a que la llamada Papyrus se ejecuta bien.
- **`akSource` importa para el volumen/audibilidad — caso real confirmado**: pasar `Self` (la
  propia referencia del Trigger Box) como `akSource` puede no oírse si el trigger es grande y el
  jugador lo cruza lejos del punto de origen de esa referencia — es audio posicional, se atenúa con
  la distancia real al `akSource`, no al volumen del trigger en sí. Pasar `PlayerRef` como
  `akSource` en su lugar garantiza que suene siempre (distancia cero al oyente), a costa de perder
  el posicionamiento espacial del sonido. Confirmado en el juego en este proyecto: con `Self` no se
  oía nada, con `PlayerRef` sí.

### El marcador de destino (punto B) es un XMarker, no un Activator invisible

Fuente: `ck.uesp.net` ("Static", "Markers"), vía snippets de `WebSearch`.

- `XMarker` es un objeto `Static` marcado como **Is Marker**: sus referencias son invisibles en el
  juego, sin colisión, no se pueden escalar (quedan fijas a 1.0000), y son válidas como parent para
  "enable references".
- Citado literalmente como el uso genérico de los markers en general: *"where something is moved
  to during some quest"* — es decir, `XMarker` es el objeto pensado exactamente para este caso (un
  punto de destino fijo, sin necesidad de volumen de trigger ni de script propio).
- **Distinto del Activator invisible ya documentado arriba** ("Activator invisible como base de un
  Trigger Box") — ese patrón (`Activator` sin `Model`, `Is Marker` + `Ignored By Sandbox`) es
  específico para cuando además hace falta adjuntar una `Primitive` de volumen y/o un script
  (el Trigger Box). Para un simple punto de posición/rotación sin volumen ni script, `XMarker` ya
  existe hecho para eso — no crear un Activator nuevo para este rol.

### Patrón completo aplicado

1. **Punto A**: la posición/rotación donde se coloca el Activator con el modelo de la criatura al
   arrastrarlo al render window — no hace falta ningún marcador aparte para el origen.
2. **Punto B**: un `XMarker` (ya existente en Object Window → WorldObjects → Static) colocado y
   rotado en el destino.
3. **Trigger Box**: patrón ya documentado arriba (Activator invisible + Primitive), con un script
   propio (extiende `ObjectReference`) con properties `ObjectReference CreatureRef`,
   `ObjectReference TargetMarker` (el XMarker) y `Actor PlayerREF` (Auto-Fill) — en
   `OnTriggerEnter`, comprobar `akActionRef == PlayerREF`, llamar
   `CreatureRef.TranslateToRef(TargetMarker, afSpeed)` y `Self.Disable()` para que no vuelva a
   dispararse.
4. **Llegada a B**: no hace falta ningún `Stop()` — `TranslateToRef` se detiene sola al alcanzar la
   posición/rotación del target. Para reaccionar al instante exacto de llegada (sonido, animación,
   avanzar un stage), un script en el propio `CreatureRef` con `Event OnTranslationComplete()`.

## Fallout 4 vs Skyrim — no asumir paridad de features

`ck.uesp.net` aloja tutoriales tanto del CK de Skyrim como del de **Fallout 4** (p. ej. "Fallout 4
Simple Fetch Quest Tutorial", una serie de capítulos numerados sobre cómo montar una fetch quest
completa en el CK de Fallout 4). Son editores de la misma familia pero **no idénticos** — Fallout 4
añadió funcionalidad de aliases que no está confirmada para el CK de Skyrim (la más relevante para
este tema: un tipo de alias de "colección" que agrupa varias referencias). Si una búsqueda te trae
contenido de una página con "Fallout 4" en el título/URL, no lo apliques a Skyrim sin decir
explícitamente que viene del CK de otro juego y que no se ha confirmado que exista igual aquí.

## Hacer que una criatura hable con el jugador (gallina, perro, atronach...) — investigado 2026-09-18

Contexto: la corteza del material 3 la da una gallina por diálogo. Por defecto **las criaturas no
entran en diálogo** — hay que habilitarlo explícitamente. Estado de cada dato según la jerarquía
de fuentes de esta skill:

**Verificado contra fichero/API real (nivel más alto):**
- `defaultAllowPCDialogueScript` existe en el juego base
  (`D:\Modlists\SME\Stock Game\Data\Scripts\Source\defaultAllowPCDialogueScript.psc`). Contenido
  completo: `Scriptname defaultAllowPCDialogueScript extends Actor`, doc `{script will make this actor
  able to talk to the PC}`, y un único `event OnLoad()` → `AllowPCDialogue(true)`.
- `Actor.AllowPCDialogue(bool abTalk)` es nativa (bellcube): *"Flags this actor as being able to talk
  to the player or not (overrides the race flag)."* — o sea, sobreescribe el flag de la Race.
- Fragmento de un Topic Info: `Function Fragment_0(ObjectReference akSpeakerRef)` (+ `Actor akSpeaker =
  akSpeakerRef as Actor`), extiende `TopicInfo`, y `GetOwningQuest()` está disponible (vanilla
  `TIF__0001E1AC.psc`). Precedente exacto de "dar un MiscObject + completar objective desde un
  diálogo": `TIF__0001F6CB.psc` (`Game.GetPlayer().AddItem(pDB01Plate,1)` + `SetObjectiveCompleted` +
  `SetStage`). Los fragmentos vanilla usan `Game.GetPlayer()` directamente.
- `Quest.IsObjectiveCompleted(int aiObjective)` existe (bellcube).
- La gallina vanilla tiene Base ID `000A91A0` (UESP `Skyrim:Chicken`); el Editor ID no figura ahí.

**Fuentes de comunidad (no oficiales — tratar como tales):**
- Tutorial `skyrimmw.weebly.com/skyrim-modding/making-a-creature-talk-skyrim-modding-tutorial`
  (abierto entero): (1) Voice Type propio — *Character > Voicetype, Right Click > New*, género acorde
  a la criatura, asignarlo al NPC; (2) duplicar la Race de la criatura (*Character > Race → Duplicate*),
  cambiar `editorID`, marcar **"Allow PC Dialogue"** ("critical step"), opcional "Uses Headtrack
  Anims", `Morph Race` y `Armor Race` apuntando a la Race original, asignar la nueva Race al NPC;
  (3) opcional: keyword `ActorTypeNPC` (pestaña Keywords del Actor). Aviso del propio tutorial:
  el lipsync no funciona salvo que la criatura ya traiga datos para ello.
- Resumen de WebSearch (las páginas de origen dan 403 a WebFetch: `wiki.beyondskyrim.org` "Voice Line
  Implementation" y foros de Nexus, no abiertas): para un caso puntual (p. ej. un solo atronach que
  habla), añadir `defaultAllowPCDialogueScript` al NPC — vale ponerlo en el Actor base, en la
  referencia colocada, o vía alias. Existe un mod "Script Fix - defaultAllowPCDialogueScript" (Nexus
  104004) cuyo snippet dice que `OnLoad` a veces no se dispara y la criatura queda sin poder hablar
  — **no verificado**, pero es el motivo para tener el plan B de la Race.
- Hilo Steam de la CK: solo aporta "cambia el Voice Type" (débil).

**Sistema de diálogo de la CK (solo snippets de WebSearch sobre `ck.uesp.net`, dominio bloqueado a
fetch):**
- Pestaña **Player Dialogue** de la quest: Branch → Topic → Info. Un Topic es una pila de Infos; se
  usa **la primera Info cuyas conditions se cumplen**. *Topic Text* = lo que el jugador elige en la
  lista; *Response Text* = lo que responde el NPC. En una rama **Top-Level**, el prompt del Starting
  Topic es el que aparece en la lista.
- **Say Once**: la Info solo la puede decir una vez un Actor dado. Aviso: en quests **sin Start Game
  Enabled**, la Say Once se resetea al salir de Skyrim — usar una property/flag propia si es crítico.
- Flags de Info: **Has LIP File** ("usually checked for Actors and left unchecked for
  TalkingActivators"); **Force Subtitle** (el subtítulo se muestra siempre, sin importar la distancia
  al hablante).
- Restringir una línea a un NPC concreto: condition `GetIsID` (form base) o `GetIsAliasRef` (alias);
  ambas citadas como alternativas en foros/wiki de Beyond Skyrim.
- Líneas sin fichero de voz: se muestran como subtítulos (fuente: descripción del mod Fuz Ro D'oh,
  que genera .fuz silenciosos y requiere SKSE — no necesario aquí).

**Sin verificar — comprobar en el juego:** que activar la gallina ofrece "Talk"; que el diálogo abre
sin una línea Hello propia; el comportamiento de la IA de la gallina (huir/deambular) al acercarse;
que una Info sin voz ni LIP se muestra bien en una criatura sin datos faciales; si `Essential`/
`Protected` (para que no se pueda matar y dejar el material inalcanzable) está en la pestaña Traits del
Actor y cómo se llama exactamente el checkbox.

## Guardián que obliga a hablar y castiga con un hechizo (Fus Ro Dah) — investigado 2026-09-18

Diseño de la quest: un animal obliga al jugador a hablar; dar una verdura lo deja pasar, cualquier otra
cosa (o cerrar el diálogo con TAB) le lanza un Fus Ro Dah. Ver `QUEST-DESIGN.md` → "Material 3".

**Verificado contra fichero/API real:**
- `Spell.Cast(ObjectReference akSource, ObjectReference akTarget=NONE)` (bellcube): *"The function works
  regardless of whether akSource actually possesses the spell; it even functions on non-actor
  objects."* Caveats citados: instantáneo y **no anima al actor**; si el objetivo es un actor apunta a su
  nodo de cabeza; no se puede llamar con origen/objetivo en una celda descargada; invalida un
  `isDualCasting` en curso sobre el origen. Para casteo "normal" de actores, la propia página remite a
  paquetes de IA con `UseMagic`.
- `Actor.DoCombatSpellApply(Spell akSpell, ObjectReference akTarget)` (bellcube): *"Adds the specified
  Spell to the target actor from this one as the caster."* — alternativa sin proyectil.
- `ObjectReference.Activate(ObjectReference akActivator, bool abDefaultProcessingOnly=false)` (bellcube):
  hace que `akActivator` active esa referencia. Aviso: *"The player cannot activate a ref whose base form
  has no name, unless that base form is a TalkingActivator."* La página **no** documenta si activar un
  NPC por script abre el diálogo — sin verificar.
- `bool Function IsInDialogueWithPlayer() native` está declarada en `ObjectReference.psc` (no en
  `Actor.psc`): *"Is this actor or talking activator currently talking to the player?"* Patrón de
  sondeo vanilla: `while (akSpeaker.IsInDialogueWithPlayer()) Utility.Wait(0.5)` en
  `DLC2DialogueRRQuestScript.psc`; también en `BardSongsScript.psc`, `DLC1SurgeryScript.psc`.
- `Actor.GetDialogueTarget()` existe (bellcube): devuelve el actor con el que este actor está en diálogo.
- Unrelenting Force (UESP `Skyrim:Unrelenting_Force`): shout `00013E07`; palabras `00013E22`/`00013E23`/
  `00013E24`; hechizos por nivel `00013E09` (Fus, tambalea) / `00013F39` (Fus Ro) / `00013F3A` (Fus Ro
  Dah, "throws enemies", ragdoll). La página no da Editor IDs. Los draugr lo lanzan y pueden tirar al
  jugador; algunos enemigos son inmunes (dragones, autómatas, ciertos NPCs).
- `FormList.GetSize()` / `GetAt(int)` y `ObjectReference.GetItemCount(Form)` /
  `RemoveItem(Form, int aiCount=1, bool abSilent=false, ObjectReference akOtherContainer=None)` leídos
  en los `.psc` del `Stock Game`.
- Verduras vanilla (UESP `Skyrim:Food`): Cabbage `00064B3F`, Carrot `00064B40`, Gourd `0010D666`,
  Leek `000669A5`, Potato `00064B41`, Tomato `00064B42`; Ash Yam `0206E7` (Dragonborn). Las manzanas
  figuran como fruta.

**Fuentes de comunidad / resúmenes de WebSearch (páginas de origen con 403, no abiertas):**
- Paquete **ForceGreet** (Package Template, CK Wiki): el NPC camina hasta el jugador e inicia el
  diálogo; campo "ForceGreet Distance"; se elige el primer Topic. Un hilo apunta a que un diálogo
  totalmente bloqueado (sin poder salir) exige scripting adicional o Scenes.
- `GetItemCount` acepta una **FormList** como parámetro y devuelve la suma de los items de la lista, así
  que `GetItemCount FormList > 0` sirve de condition de diálogo.

**Sin verificar:** que `NPC.Activate(Game.GetPlayer())` abra el diálogo; qué ocurre con
`IsInDialogueWithPlayer()` justo tras el `Activate` (probable retardo → esperar con timeout antes de
dar el diálogo por no iniciado); el nombre exacto del campo "Run On" en la condition; que el flag
Goodbye de un Topic Info cierra la conversación.

### Qué controla la fuerza del Fus Ro Dah (verificado leyendo `Skyrim.esm` y los `.psc` vanilla, 2026-09-19)

- El hechizo `VoiceUnrelentingForce3` (Fus Ro Da) tiene 4 efectos. Los dos de empuje son
  `VoiceUnrelentingForceEffect03` (`0007F82F`, "Strong") y `VoiceUnrelentingForceEffect02` (`0007F82E`,
  "Middle"). Ambos son **archetype 33 = Stagger** (UESP `Mod_File_Format/MGEF`), delivery Aimed, Fire and Forget.
- **El lanzamiento no lo hace la magnitud.** El efecto Strong lleva adjunto el script
  `VoicePushEffectScript` (`extends ActiveMagicEffect`): `Event OnEffectStart(actor Target, actor Caster)` →
  `Caster.PushActorAway(Target, PushForce)`, con `int Property PushForce` = **15** en el VMAD del MGEF. La
  magnitud del efecto en el hechizo solo afecta a la parte de tambaleo (Stagger).
- `ObjectReference.PushActorAway(Actor akActorToPush, float aiKnockbackForce)` (bellcube): *"Knocks back the
  specified actor away from this object with the specified amount of force, as if an explosion had gone
  off."* Controlado por los game settings `fMagicExplosion*`; se puede usar un `XMarkerHeading` como origen
  para controlar la dirección; **no comprobar `Is3DLoaded()` en actores recién creados puede crashear**. La
  documentación no dice si perks o resistencias afectan al empuje.
- **Qué puede bloquear el efecto Strong**: ambos efectos llevan una condition (función #560, `== 0` en Strong y
  `== 1` en Middle) sobre el keyword `ImmuneStrongUnrelentingForce` (`000172AC`). En los masters vanilla
  (Skyrim, Update, Dragonborn, Dawnguard) solo lo llevan NPC_ concretos (los cuatro Greybeards, los reyes
  nórdicos antiguos, Tsun, Karstaag, Miraak y el Segador de la Cueva de Almas): **ninguna race, armadura, perk,
  hechizo, arma ni encantamiento** lo tiene. Un mod del modlist podría añadirlo (no comprobado).
- Consecuencia práctica: para un empuje fuerte y fiable, llamar directamente a `Self.PushActorAway(alvo,
  fuerza)` desde el propio script evita la condition del keyword, la magnitud y los efectos mágicos.

### `SetStage` con un stage más bajo que el más alto ya hecho (bellcube / CK Wiki, 2026-09-19)

`Quest.SetStage(int)` devuelve `true` si el stage **existe** y se pudo poner (si no existe, `false` y nada cambia) y es
**latente** (espera a que la quest arranque y a que terminen los fragmentos del stage). Sobre poner un stage más bajo:
*"you can't set the current stage number to a lower value"* (para eso haría falta `Reset()`), *"although … this function
can still display the journal entry and run script fragments from lower numbered stages, if they hadn't previously been
completed."* En `Quest.psc` vanilla, `GetStage()`/`GetCurrentStageID()` devuelven el **stage completado más alto**. La
página no dice explícitamente que `GetStageDone` pase a `true` en ese caso; se verifica por consola:
`setstage <quest> 20`, `setstage <quest> 9`, `getstage <quest>` (→ 20), `getstagedone <quest> 9` (→ 1).
