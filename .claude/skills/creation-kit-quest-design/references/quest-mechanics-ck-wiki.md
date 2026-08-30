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

## Fallout 4 vs Skyrim — no asumir paridad de features

`ck.uesp.net` aloja tutoriales tanto del CK de Skyrim como del de **Fallout 4** (p. ej. "Fallout 4
Simple Fetch Quest Tutorial", una serie de capítulos numerados sobre cómo montar una fetch quest
completa en el CK de Fallout 4). Son editores de la misma familia pero **no idénticos** — Fallout 4
añadió funcionalidad de aliases que no está confirmada para el CK de Skyrim (la más relevante para
este tema: un tipo de alias de "colección" que agrupa varias referencias). Si una búsqueda te trae
contenido de una página con "Fallout 4" en el título/URL, no lo apliques a Skyrim sin decir
explícitamente que viene del CK de otro juego y que no se ha confirmado que exista igual aquí.
