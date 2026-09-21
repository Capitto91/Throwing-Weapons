# QUEST-DESIGN.md

Registro del estado real de la quest `CAP_ThorMjolnir_Quest_01` en la Creation Kit (`ThorMjolnirOAR.esp`).
Este archivo es la fuente de verdad para no tener que resumir la CK en cada conversación — actualízalo
en la misma tarea en que cambies algo de la quest (stage nuevo, alias nueva, script nuevo/modificado).
Si un dato de aquí entra en conflicto con lo que se ve en la CK, gana la CK — este archivo puede quedarse
desactualizado si se te olvida tocarlo.

**Última sincronización completa con la CK y los `.psc`: 2026-09-18** (capturas de Stages 12/25, Objectives y Aliases + lectura de los `.psc` de `D:\Modlists\SME\overwrite\Scripts\Source\`). Lo marcado *(planificado)* todavía no existe en la CK.

## Stages

| Stage | Disparador | Log Entry (resumen) | Fragmento Papyrus |
|---|---|---|---|
| 0 | — | (no arrancada) | — |
| 10 | Cruzar el Trigger Box del altar | "Has encontrado un altar antiguo y misterioso" | `SetObjectiveDisplayed(10)` ⚠ Cambio previsto 2026-09-20: el fragmento solo muestra el objective 10 si `GetStage() <= 10` (primera interacción con la quest); antes lo mostraba siempre y aparecía tarde, sin que nadie lo cerrara, si el altar se pisaba después de otros stages. |
| 12 | Entrar por primera vez al Trigger Box del orbe (`CAP_ThorMjolnir_Trigger_AtronachAtack`, solo si `GetStage() < 12`) | "You find yourself standing before a strange structure, with a mysterious orb at its center…" (cima de la montaña) | **Sin fragmento** (confirmado en la CK 2026-09-18) — `SetObjectiveDisplayed(14)` lo hace el propio script del trigger |
| 15 | Recoger un material **sin** haber leído el mural (camino alternativo) | Texto propio reconociendo que encontró el material sin conocer la leyenda completa | `SetObjectiveCompleted(10)` + `SetObjectiveDisplayed(11)` + `(12)` + `(13)` |
| 20 | Activar el Mural | "Has descubierto un mural en la Garganta del Mundo..." (leyenda de Sindri y Brokk, 3 materiales) | `SetObjectiveCompleted(10)` + `SetObjectiveDisplayed(11)` + `(12)` + `(13)` |
| 25 | Entrar al mismo Trigger Box con `GetStage()` entre 15 y 24 (ya tenía un material, o leyó el mural) | "You've found one of the objects described in the mural. But something feels off… It looks suspiciously like a trap. As any good adventurer knows, curiosity can be dangerous. You decide to proceed with caution." | **Sin fragmento**; el objective 14 lo muestra igualmente el script del trigger |
| 30 | Los tres materiales conseguidos, **en cualquier orden**: cada recogida (`MaterialTraker` para los materiales 1 y 3, `AtronachActivator` para el 2) comprueba que los objectives 11, 12 y 13 estén completados y, si sí, pone el stage 30 *(planificado)* | Log (creado por el usuario 2026-09-20): "You have gathered every material required. Now, return to the Throat of the World and forge the legendary weapon" | Fragmento previsto: `SetObjectiveDisplayed(17)` |

**Por qué el 15 existe**: cubre tanto "no ha tocado la quest para nada" (stage 0) como "cruzó el altar pero no leyó el mural" (stage 10) con una sola condición (`GetStage() < 15`) en los scripts de los materiales — ver sección Scripts. Leer el mural después de haber pasado por el 15 es seguro (`SetStage`/`SetObjectiveDisplayed`/`SetObjectiveCompleted` son idempotentes sobre algo ya completado/mostrado).

**Stages 12 y 25 (añadidos 2026-09-15)**: los dos los pone el script del Trigger Box del orbe (escena del material 2), no un fragmento. `GetStage() < 12` → 12 (primera visita, sin material ni mural); `15 ≤ GetStage() < 25` → 25 (ya tenía un material, o leyó el mural — el Stage 20 también cae en ese rango). Con el stage entre 12 y 14 no hace nada. Ninguno de los dos tiene fragmento Papyrus. **Observación**: en ambos textos de log falta el espacio tras el punto ("mountain.But", "trap.As") — comprobar en la CK si es real.

## Objectives

| Índice | Display Text | Target (marcador) | Notas |
|---|---|---|---|
| 10 | "Investigate the ancient, mysterious altar." | `MuralAlias` (con marcador) | Único objective con marcador de mapa |
| 11 | "Find the mineral in the depths of the North Sea." | Ninguno | **Sin marcador, deliberado** — el jugador debe encontrarlo por su cuenta o vía la pista del NPC dwemer |
| 12 | "Take bark from the tree that is older than the world itself." | Ninguno | Sin marcador, igual que el 11 |
| 13 | "Harness the essence of lightning." | Ninguno | Sin marcador, igual que el 11 |
| 14 | "Investigate the mysterious orb." | *(sin verificar — la captura de la CK solo mostraba el Target del 10)* | Lo muestra `Trigger_AtronachAtack` (`SetObjectiveDisplayed(14)`, primera entrada); lo completa `AtronachActivator.OnActivate` al dar el material 2 |
| 15 | "Defeat the guardian of the orb." | *(sin verificar)* | Lo muestra `AtronachActivator.OnActivate` la primera vez (atronach deshabilitado → `Enable()`); lo completa `AtronachVulnerability.OnDeath` |
| 16 | *(planificado 2026-09-19)* "Get past the furious rooster." (borrador) | Ninguno | Lo muestra `RegisterEncounterStage()` del script de la gallina en cada encuentro (idempotente, así también aparece en partidas que ya tenían el stage 11/16); lo completa la rama "prueba superada", o `Vanish()` si la corteza se consigue antes de pasar la prueba |
| 17 | "Return to the Throat of the World to forge the legendary weapon." (creado por el usuario 2026-09-20; el plan decía 20) | `ForgeALias` (sin condition) | Se mostrará en el fragmento del stage 30; se completará al forjar |

Dejar la lista de Targets vacía en un Objective es lo que evita el marcador — no hace falta ningún flag "sin marcador", el marcador solo aparece si se añade una fila de Target apuntando a una Alias.

## Aliases

| Nombre | Tipo | Fill Type | Quest Object | Notas |
|---|---|---|---|---|
| `MuralAlias` | Reference | Specific Reference → Activator del Mural | No | Usada solo como target del Objective 10 |
| `AsteroideAlias` (material 1) | Reference | Specific Reference → Misc Item `CAP_ThorMjolnir_Material_AsteroidOre`, colocado directamente en el mundo | **Sí** | `Optional` marcado. Se coge con la interacción normal del motor; la protección de Quest Object viene de esta Alias. Además, el objeto lleva su propio script `CAP_ThorMjolnir_MaterialTraker` (ver Scripts) para completar el objective y cubrir el camino alternativo (Stage 15). Nombre real confirmado leyendo `QF_CAP_ThorMjolnir_Quest_01_0101E219.psc` (property `Alias_AsteroideAlias`) |
| `Stormheard Alias` (con espacio en la CK; property `Alias_Stormheard_Alias`; ID 2; material 2, el Rayo) | Reference | **Specific Reference, sin nada seleccionado** (a propósito — empieza vacía) | **Sí** | **`Optional` marcado — imprescindible.** Patrón distinto de `AsteroideAlias`: el material no existe como referencia colocada de antemano (lo suelta un jefe al morir), así que no se puede fijar un Specific Reference al diseñar la quest. Se rellena en tiempo real por script vía `ReferenceAlias.ForceRefTo()` justo antes de mandarlo al inventario del jugador — ver `CAP_ThorMjolnir_AtronachActivator` en Scripts |
| `StormheardActivatorAlias` (ID 3) | Reference | Specific Reference, sin nada seleccionado (empieza vacía) | No (solo flag `O`) | `Optional` marcado. La rellena `CAP_ThorMjolnir_Trigger_AtronachAtack` con `ForceRefTo(ActivadorRef)` la primera vez que el jugador entra al trigger del orbe. Añadida 2026-09-15, confirmada en la CK 2026-09-18 |
| `StormheardAtronachAlias` (ID 4) | Reference | Forced → `CAP_ThorMjolnir_NPC_EncAtronachStorm` (`030A9F9C`) | No | **No `Optional`** (forzada a una referencia real, no vacía). Añadida 2026-09-15, confirmada en la CK 2026-09-18 |
| `RootAlias` (ID 6, material 3, la raíz) | Reference | Specific Reference → `REFR 030E48A7` (base `CAP_ThorMjolnir_Material_MimameidrRoot`, MISC) | **Sí** | `Optional` + Quest Object (leído del `.esp` 2026-09-20). Es el **target del objective 12**, con la condition `GetStageDone(9) == 1` en su fila |
| `ChickenAlias` (ID 5) | Reference | Specific Reference → la gallina (`ACHR 030E19C7`) | No (solo `Optional`) | Añadida por el usuario; es el **target del objective 16** |
| `ForgeALias` (ID 7, con esa mayúscula) | Reference | Specific Reference → `REFR 030E48AA` (base `CAP_ThorMjolnir_Activator_Forge`, ACTI) | No | Sin flags. Es el **target del objective 17**. Aún sin script en la forja |

**Trampa real ya sufrida (2026-09-14), checkbox `Optional` de una Alias**: si una Alias tiene
`Fill Type = Specific Reference` sin nada seleccionado (el patrón de "se rellena más tarde por
script", ver `StormheardAlias`) y **no** está marcada `Optional`, la quest entera falla al
arrancar — no solo esa alias. Confirmado en el juego con instrumentación (`Debug.Trace` en
`CAP_ThorMjolnir_MaterialTraker`): con la alias sin `Optional`, `MyQuest.IsRunning()` daba
`False`, `IsStopped()` daba `True`, y tanto `MyQuest.Start()` como `MyQuest.SetStage(15)` volvían
`False` sin ningún error visible en `Papyrus.0.log` — un fallo completamente silencioso. Fuente:
`ck.uesp.net/wiki/Quest_Alias_Tab` (vía WebSearch, dominio bloqueado a fetch directo): *"si
`Optional` está marcado, la quest no necesita rellenar esa alias para arrancar; si no, la quest
falla al arrancar si no puede rellenarla — y si una sola alias no-opcional no se rellena, ninguna
alias se rellena y la quest no arranca"*. Cualquier Alias nueva que empiece vacía a propósito
(rellenada luego por `ForceRefTo()` o similar) **tiene que llevar `Optional` marcado**, sin
excepción.

## Lógica de stages — mapa y revisión (2026-09-19)

Fuente: lectura del `.esp` guardado (volcado de solo lectura), de los `.psc` y del log. **Regla base**: `Quest.GetStage()` devuelve el stage completado **más alto** (✅ `Quest.psc` vanilla), no el último puesto; los stages sin fragmento solo añaden su entrada de diario.

| Stage | Lo pone | Condición | Efecto |
|---|---|---|---|
| 9 | Script de la gallina, tras cerrarse el diálogo con el tributo entregado *(planificado)* | `bPassed` | Solo log; es el interruptor de las condiciones de diálogo de la gallina (`GetStageDone(9) == 0`). Número < 12 a propósito |
| 10 | Trigger del altar (`defaultSetStageTrigSCRIPT` vanilla: `Start()` si no corre + `SetStage(10)`; `doOnce=True`, `disableWhenDone=True`, sin `prereqStageOPT`) | primera entrada del jugador, sin importar el progreso | log + Fragment_0: `SetObjectiveDisplayed(10)` ⚠ Cambio previsto 2026-09-20: el fragmento solo muestra el objective 10 si `GetStage() <= 10` (primera interacción con la quest); antes lo mostraba siempre y aparecía tarde, sin que nadie lo cerrara, si el altar se pisaba después de otros stages. |
| 11 | Gallina (`StartEncounter`) | ni 15 ni 20 hechos; una vez (`!GetStageDone(11)`) | solo log |
| 12 | Trigger del orbe | `GetStage() < 12`; primera vez | solo log (el objective 14 lo muestra el script) |
| 15 | `MaterialTraker` (mat. 1) y `AtronachActivator` (mat. 2) | `GetStage() < 15` | log + Fragment_5: completa obj 10, muestra 11–13 |
| 16 | Gallina (`StartEncounter`) | 15 o 20 hechos; una vez (`!GetStageDone(16)`) | solo log |
| 20 | Mural | `GetStage() < 20` (el mensaje sale siempre) | log + Fragment_3 (idéntico al 15) |
| 25 | Trigger del orbe | `15 ≤ GetStage() < 25` | solo log (el objective 14 lo muestra el script) |
| 30 | Los scripts de recogida, al completarse los tres objectives *(planificado)* | los objectives 11, 12 y 13 completados | Log + fragmento previsto `SetObjectiveDisplayed(17)` |

Cableado comprobado en el `.esp` (propiedades reales): altar (stage 10, doOnce), Mural, `MaterialTraker` (`ObjectiveIndex = 11`), trigger y Activator del orbe, atronach, y la gallina (`ACHR 030E19C7` con el script `CAp_ThorMjolnir_ShallnotPass`, `FusRoDahSpell = CAP_ThorMjolnir_Spell_ShallnotPass` — copia propia del hechizo vanilla, **con** sus efectos de daño: Generic Damage 10 y Disintegrate 40/1 s) y su trigger (`ChickenRef` → esa ACHR). Fragmentos del quest: 10→Fragment_0, 15→Fragment_5, 20→Fragment_3; 11, 12, 16 y 25 sin fragmento.

**Riesgos detectados en la revisión (no son bugs confirmados)**:

- **R1 — carrera fin de diálogo / `MarkPassed()`**: el fragmento End de la Info de la verdura puede ejecutarse un instante después de que el sondeo vea que el diálogo cerró → castigo injusto a quien respondió bien. Arreglo: periodo de gracia (~1 s) antes de castigar y volver a comprobar `bPassed`. **Aplicado 2026-09-19** (`GraceTime = 1.0` s antes de castigar). **Revertido por el usuario 2026-09-20**: se quitó el periodo de gracia (riesgo residual bajo: en la prueba real el fragmento corrió antes que el sondeo). Mitigación opcional sin retardo: mover el fragmento de la Info del tributo de End a Begin.
- **R2 — `bRunning` atascado**: si la gallina se descarga o se guarda/carga a mitad de encuentro, `bRunning` puede quedarse a `true` y no habría más encuentros. Arreglo: resetearlo en `OnLoad`. **Aplicado 2026-09-19** (`OnLoad` resetea el estado transitorio).
- **R3 — doble arranque**: `SetStage` es potencialmente latente; `bRunning = true` debe ponerse **antes** de `RegisterEncounterStage()`. **Aplicado 2026-09-19** (`bRunning = true` antes de `RegisterEncounterStage()`).
- **R4 — arranque de la quest**: el vanilla hace `Start()` explícito antes de `SetStage`; el resto de nuestros scripts confían en que `SetStage` la arranque (funciona en el juego, es el camino alternativo ya probado). Para la gallina, añadir el `Start()` defensivo. **Descartado por el usuario 2026-09-19**: el camino por `SetStage` ya le ha funcionado, no se cambia.
- **R5 — futuro Stage 30**: cualquier stage ≥ 25 puesto antes de la primera visita al orbe rompe su detección (`< 12` / `15–24`). Antes de crear el 30, pasar el orbe a `GetStageDone`.
- **R6 — objectives (❓ la documentación no especifica)**: `SetObjectiveDisplayed` sobre un objective ya completado (fragmentos 15/20 tras completar el 11; Fragment_0 si el altar se pisa tarde) y `SetObjectiveCompleted(10)` sobre uno nunca mostrado (camino alternativo). Probar en el juego: material 1 → mural (el 11 debe seguir completado); material → altar (el 10 debe seguir completado).
- **R7 — daño del castigo**: la copia del hechizo conserva Generic Damage 10 y Disintegrate 40/1 s; con reintentos ilimitados puede matar a un jugador de poca vida. **Resuelto 2026-09-19**: la copia del hechizo ya no tiene efectos de daño.
- **R8 — nivel**: si el jugador activa a la gallina sin cruzar antes el trigger y la quest está parada, "Talk" no hace nada.
- **R9 — plazo por tiempo real que no sobrevive a una sesión nueva** (revisión de scripts 2026-09-21): `AtronachVulnerability` guarda `vulnerableDeadline = Utility.GetCurrentRealTime() + 15` en una variable de script (se guarda en el save), pero `GetCurrentRealTime()` cuenta segundos **desde que se lanzó el juego** (📖 CK Wiki vía bellcube). Si se guarda con la fase abierta y se carga en otra sesión, el plazo puede quedar muy en el futuro: el atronach seguiría vulnerable y sin IA hasta que le bajen un tercio de vida (inferencia, no reproducida). **Primer intento (2026-09-21) — NO funcionó**: `Event OnLoad()` que cerraba la fase si `bIsVulnerable`. Probado en el juego (guardar con la fase abierta, cerrar, volver a cargar): el atronach siguió vulnerable "un buen rato" y `Papyrus.0.log` no contiene la traza. La CK Wiki avisa de que `OnLoad` *"doesn't fire reliably for references that load 3D while or immediately after the player loads a savegame"* (📖 vía bellcube). La cadena de `OnUpdate` sí sobrevivió al guardado (acabó cerrándose sola). **Arreglo aplicado en el `.psc` (2026-09-21) y probado en el juego: funciona** (fase abierta, guardar, cerrar, cargar → el atronach vuelve a ser invulnerable): en `OnUpdate`, si `vulnerableDeadline - ahora > VulnerablePhaseTimeout + 1.0` el plazo es de otra sesión → traza `[CAP_ThorMjolnir][Vulnerability] plazo de otra sesion (...)` + `EndVulnerability()`; el `OnLoad` se quitó. Verificado compilando al scratchpad (0 errores/avisos). Para que la prueba discrimine: abrir la fase tarde en la primera sesión (~2 min tras lanzar el juego) y cargar pronto en la segunda; si el plazo ya hubiera vencido, la lógica antigua lo cierra sin traza. ⚠️ `ShallnotPass` (R2) también descansa en un `OnLoad` para resetear `bRunning`: mismo evento poco fiable, no probado.
- **R10 — activar el Stormheart muy rápido daba dos materiales (confirmado en el juego 2026-09-21)**: `AtronachActivator.OnActivate`, con el atronach muerto, encadena `PlaceAtMe` → `ForceRefTo` → `AddItem` → objectives/stages → `Self.Disable()`, y hasta ese `Disable()` nada impedía una segunda activación (cada llamada externa puede ceder el control del script; ⚠️ nota *Threading Notes* de la CK Wiki, solo vista como snippet). **Arreglo aplicado en el `.psc` (2026-09-21), pendiente de compilar en la CK y probar**: variable `bMaterialGiven` comprobada y asignada de forma consecutiva **antes** de `PlaceAtMe` (verificado en el bytecode: sin llamadas externas entre las dos); si `PlaceAtMe` devuelve `None` se reabre la bandera, para no dejar al jugador sin material. Trazas `[CAP_ThorMjolnir][Activator]`: "segunda activacion ignorada" y "PlaceAtMe no devolvio el material". La bandera se guarda en el save: para repetir la prueba hay que cargar uno anterior a coger el material.

**No probado todavía**: que `Activate(PlayerRef)` abra el diálogo con la quest en marcha; que el Hello (`IsInDialogueWithPlayer == 1`) se diga; la opción "empujar" (sigue sin Info en el `.esp`); que `GetStageDone(9)` pase a `true` tras un `SetStage(9)` con el jugador ya en stage ≥ 20 (prueba de consola: `setstage <quest> 20`, `setstage <quest> 9`, `getstage` → 20, `getstagedone <quest> 9` → 1).

## Cómo verificar esto contra la realidad

Los Stages/Aliases/Objectives viven solo en el `.esp` binario compilado — sin herramienta dedicada
(SSEEdit con script Pascal, instalado en `D:\Modlists\SME\tools\SSEEdit` pero sin automatizar
todavía) no se pueden leer directamente, así que esta sección de arriba depende de que se actualice
a mano. **Los scripts Papyrus sí son texto plano verificable directamente**: los `.psc` reales de
este proyecto están en `D:\Modlists\SME\mods\ThorMjolnir_OAR\Scripts\Source\` (movidos ahí desde la
carpeta "overwrite" de Mod Organizer 2, que ya no tiene `Scripts`; comprobado 2026-09-21) — antes de asumir el contenido de un script, léelo de ahí en vez de fiarte solo de
esta tabla o de lo que se recuerde de la conversación.

**Dónde caen los archivos de la CK (MO2)**: los archivos **nuevos** que crea la CK (los `.pex`/`.psc` de scripts nuevos) los deja MO2 en `overwrite`; los que ya existen en un mod se modifican donde están (por eso el `.esp` sí va a `mods\ThorMjolnir_OAR`). Se mueven a mano al mod (con la CK cerrada, arrastrar `Scripts` de `Overwrite` al mod; una sola vez por script). **No mover `Root\`** (ficheros de la CK/CKPE, logs, capturas, backups `.bak`) **ni `Scripts\Source\temp`**. Funciona igual desde `overwrite` (máxima prioridad); moverlos es por orden y para empaquetar el mod.

## Scripts

| Script | Adjunto a | Properties | Rol |
|---|---|---|---|
| `CAP_ThorMjolnir_Script_Mural01` | Activator del Mural | `PlayerREF` (Actor, Auto-Fill), `MiQuest` (Quest), `MuralMessage` (Message) | `OnActivate` (jugador): muestra **siempre** el `Message`; `SetStage(20)` solo si `GetStage() < 20` (cambio 2026-09-19: antes el mensaje también dependía de `< 20` y con stage ≥ 20 no salía) |
| `CAP_ThorMjolnir_MaterialTraker` (así, con esa errata — nombre real en disco) | Cada Misc Item de material colocado en el mundo (reutilizable, un `ObjectiveIndex` distinto por instancia) | `MyQuest` (Quest), `PlayerRef` (Actor), `ObjectiveIndex` (Int — `11` en el material 1) | `OnContainerChanged`: si el nuevo contenedor es el jugador, primero `If MyQuest.GetStage() < 15: SetStage(15)` (camino alternativo — cubre stage 0 y stage 10, converge con el fragmento de Stage 20 sin duplicar progreso, ver sección Stages) y después `SetObjectiveCompleted(ObjectiveIndex)`. Aplicado y verificado contra el `.psc` real en disco 2026-09-14 |
| `CAP_ThorMjolnir_Materials_Pickup` | *(sin verificar a qué está adjunto)* | `OreAlias` (ReferenceAlias), `PlayerRef` (Actor) | `OnActivate`: `PlayerRef.AddItem(OreAlias.GetReference(), 1)` + `Self.Disable()`. Existe en disco (2026-09-10) pero no estaba documentado; parece el patrón Activator descartado a favor de `MaterialTraker` para el material 1 — comprobar en la CK si sigue adjunto a algo, y si no, borrarlo | **Huérfano confirmado 2026-09-19** (no está adjunto a ningún registro del `.esp`): candidato a borrar (`.psc` + `.pex`).
| Fragmentos de Stage (`QF_CAP_ThorMjolnir_Quest_01_...`, autogenerados) | La propia Quest (extienden `Quest` directamente — llamar sus funciones **sin** prefijo de property) | — | Ver columna "Fragmento Papyrus" de la tabla de Stages |
| `CAP_ThorMjolnir_AtronachVulnerability` | NPC_ único `CAP_ThorMjolnir_NPC_EncAtronachStorm` (Storm Atronach dedicado, no el vanilla — imprescindible porque `ActorBase.SetInvulnerable` afecta a todas las referencias que compartan el mismo `ActorBase`) | `PollInterval` (Float, 0.1), `VulnerablePhaseTimeout` (Float, 15.0), `ImmuneNoticeCooldown` (Float, 3.0), `ActivadorRef` (`CAP_ThorMjolnir_AtronachActivator`), `MyQuest` (Quest), `PlayerRef` (Actor) | Ciclo de vulnerabilidad del material 2 (el Rayo): `BeginVulnerability()` abre una ventana con suelo de vida = vida actual − 1/3 de la vida máxima, `EnableAI(false)` + `ActorBase.SetInvulnerable(false)`; `OnUpdate` sondea la vida y cierra la ventana sola al llegar al suelo (restaura lo que se pasó del suelo) o pasados `VulnerablePhaseTimeout`s (`EndVulnerability()`, vuelve a `Invulnerable(true)` + `EnableAI(true)`); `OnHit`: si el agresor es el jugador y no está vulnerable, `Debug.Notification("The Atronach seems immune to your attacks.")` como mucho una vez cada `ImmuneNoticeCooldown` s (2026-09-21, probado en el juego: funciona; una marca de tiempo en el futuro se ignora por venir de otra sesión, mismo criterio que R9); `OnDeath`: `EndVulnerability()` + `MyQuest.SetObjectiveCompleted(15)` + `ActivadorRef.SetOpen(true)` (abre la animación Havok nativa open→openedloop del Activator); `OnUpdate` cierra además la fase si el plazo guardado es de otra sesión (2026-09-21, ver R9; el `OnLoad` que se probó primero se quitó porque no es fiable al cargar una partida) |
| `CAP_ThorMjolnir_AtronachActivator` | Activator central de la escena del Rayo (`CAP_ThorMjolnir_Activator_Stormheart`, tiene animaciones Gamebryo `AnimIdle01`/`AnimIdle02` y Havok `open`/`openedloop`/`close`/`closedloop` propias) | `PlayerRef` (Actor, Auto-Fill), `AtronachRef` (`CAP_ThorMjolnir_AtronachVulnerability`), `ShockHazardEffect` (MagicEffect, `HazardShockDamageFFContact`/"Shock Hazard"), `AtronachAttackSpell` (Spell, `crAtronachStormAreaAttack`), `PollInterval` (Float, 0.25), `StormheardMaterial` (MiscObject, `CAP_ThorMjolnir_Material_Stormheard`), `StormheardAlias` (ReferenceAlias → la alias `Stormheard Alias`), `MyQuest` (Quest) | `OnActivate` (solo jugador), en este orden: (1) atronach muerto → comprueba y pone `bMaterialGiven` (2026-09-21, ver R10: antes de la primera llamada externa; se reabre si `PlaceAtMe` devuelve `None`) y crea el material (`PlaceAtMe` + `StormheardAlias.ForceRefTo()` + `AddItem`) + `SetObjectiveCompleted(14)` + `If GetStage() < 15: SetStage(15)` + `SetObjectiveCompleted(13)` + `Self.Disable()`; (2) atronach deshabilitado → `AtronachRef.Enable()` + `SetObjectiveDisplayed(15)` y sale *(el atronach parte deshabilitado — inferido del script, confirmar en la CK que la referencia colocada tiene Initially Disabled)*; (3) ya vulnerable → sale; (4) si `PlayerRef.HasMagicEffect(ShockHazardEffect)` → `BeginVulnerability()` + lanza `AtronachAttackSpell` sobre el atronach + `Debug.Notification("Now it's vulnerable, take advantage!")`. `StartAnimWatch()`/`StopAnimWatch()` (llamadas desde el Trigger cuando el jugador entra/sale de la zona) arrancan/paran un sondeo que reproduce `AnimIdle01` mientras `PlayerRef.HasMagicEffect(ShockHazardEffect) \|\| AtronachRef.IsVulnerable()`, y `AnimIdle02` cuando ninguna se cumple |
| `CAP_ThorMjolnir_Trigger_AtronachAtack` | Trigger Box alrededor del Activator del Rayo / orbe | `AtronachRef` (`CAP_ThorMjolnir_AtronachVulnerability`), `ActivadorRef` (`CAP_ThorMjolnir_AtronachActivator`), `PlayerRef` (Actor, Auto-Fill), `MyQuest` (Quest), `StormheardActivatorAlias` (ReferenceAlias) | `OnTriggerEnter` (solo el **jugador**; lee el stage **una sola vez** en un local, `Int currentStage = MyQuest.GetStage()`, desde 2026-09-21 — antes hasta 3 llamadas cruzadas): si `GetStage() < 12` → `SetStage(12)`; si no y `15 ≤ GetStage() < 25` → `SetStage(25)`; en cualquiera de esos dos casos (primera vez) `StormheardActivatorAlias.ForceRefTo(ActivadorRef)` + `SetObjectiveDisplayed(14)`; siempre `ActivadorRef.StartAnimWatch()`. `OnTriggerLeave` (jugador): `ActivadorRef.StopAnimWatch()` |
| `CAP_ThorMjolnir_ForgeScript` | Pedestal / forja (`REFR 030E48AA`, alias `ForgeALias`) | 21: `PlayerRef`, `MyQuest`, `AsteroidItem`/`StormheartItem`/`RootItem` (MISC), `ShowAsteroid`/`ShowStormheart`/`ShowRoot`, `WeaponRef`, `SkyMarker` (refs), `BoltSpell`, `StormEffect1/2/3`, `FinalExplosion`, y 6 numéricas con valor por defecto (`MaxDistance` 3000, `PollInterval` 0.5, `StrikeCount` 6, `StrikeInterval` 0.5, `SkyHeight` 1800, `SkySpread` 350) | Depósito de los 3 materiales (`OnActivate`) + vigilancia de Storm Call + secuencia de rayos/explosión + aparición del arma. Ver sección "Forja y cierre" (estado 2026-09-21). Verificado en el `.esp` y probado en el juego 2026-09-21 |
| `CAP_ThorMjolnir_Trigger_Forge` | Trigger `CAP_ThorMjolnir_Trigger_TOW_001` (`REFR 0301E21D`), **sustituye** al vanilla `defaultsetStageTrigSCRIPT` | `myQuest` (Quest), `startStage` (Int, 10), `PlayerRef` (Actor, Auto-Fill), `ForgeRef` (`CAP_ThorMjolnir_ForgeScript` → `REFR 030E48AA`) | `OnTriggerEnter` (solo jugador): si `StartStage` no está hecho → `Start()` (si no corre) + `SetStage(StartStage)`; después `ForgeRef.StartStormWatch()`. **Nunca se desactiva** (el vanilla tenía `disableWhenDone`/`doOnce` = True). Verificado en el `.esp` y probado en el juego 2026-09-21 |
| `CAP_ThorMjolnir_MjolnirPickUp` | Activator base `CAP_ThorMjolnir_WeaponActivatorModel` (`03006912`; properties de la base: `PlayerRef`, `MjolnirWeapon` = `ThorMjolnir` `03000D63`) | además, **solo en la referencia** del arma (`REFR 03007E9D`, alias `MjolnirStaticAlias`): `MyQuest` (opcional) = `CAP_ThorMjolnir_Quest_01`, `CompletionStage` (opcional) = 50 | `OnActivate` (jugador): sale del estado de espera con `GoToState("Done")` **primero** (`SetStage` es latente: una segunda activación no debe dar otra arma) → `AddItem(MjolnirWeapon)` → si `MyQuest` y `CompletionStage > 0`: `SetStage(CompletionStage)` (cierra la quest con el 50) → `Disable()` + `DeleteWhenAble()`. Sin las properties (cualquier otro uso del activator) se comporta como antes. Verificado en el `.esp` y probado en el juego 2026-09-21; compilado en su sitio, dentro del mod (no cae en `overwrite`) |
| `CAP_ThorMjolnir_AtronachSpellDetector` | Magic Effect "Vulnerable" (`AtronachStormVulnerableTime`) — **histórico, descartado como mecanismo de detección**, ver más abajo | — | Ya no se usa para detectar nada; se dejó en su momento como logger puro de `OnEffectStart`/`OnEffectFinish` durante el debugging, puede quitarse del Magic Effect sin afectar al sistema actual | Comprobado 2026-09-19: sigue adjunto al MGEF `AtronachStormVulnerableTime`, que **sí está en uso** por `CAP_ThorMjolnir_Spell_AtronachStormAreaAttack` — quitar solo el script, no el MGEF.

**Nota de scripting importante**: dentro de un fragmento de Stage no existe ninguna property `MyQuest`/similar — el fragmento ya extiende `Quest`, así que se llama `SetObjectiveDisplayed(11)` directamente, nunca `MyQuest.SetObjectiveDisplayed(11)` (error de compilación "variable is undefined").

## Objetos / Misc Items

| Objeto | Rol | Notas |
|---|---|---|
| `CAP_ThorMjolnir_Material_AsteroidOre` | Material 1 (mineral de meteorito) | Tipo `MiscObject`, colocado directamente en el mundo, sin colisión propia — mecanismo de recogida es el `OnContainerChanged` de la tabla de Scripts, no un `OnActivate` |
| `CAP_ThorMjolnir_Material_Stormheard` | Material 2 (esencia del Rayo) | Tipo `MiscObject`, **nunca colocado en el mundo** — se crea por script (`PlaceAtMe`) directamente en el inventario del jugador al interactuar con el Activator tras la muerte del atronach. Protección Quest Object vía `StormheardAlias` (ver Aliases) |
| `CAP_ThorMjolnir_Material_MimameidrRoot` | Material 3 (raíz del árbol) | **MiscObject** ("Mimameidr Root", modelo `Plants\FloraCreepCluster01.nif`, valor 50, peso 1.0), colocado en el mundo (`REFR 030E48A7`) y con `MaterialTraker` (`ObjectiveIndex = 12`, `RoosterRef` = la gallina). Sustituye al INGR propio anterior `CAP_ThorMjolnir_Material_Root`, que quedó marcado como borrado en el `.esp` (decisión 2026-09-20: un ingrediente se podría comer o gastar en alquimia antes de la forja) |

## Escenario del Atronach de Tormenta (Material 2 — el Rayo)

Mecánica completa: un Storm Atronach dedicado (NPC_ único, no el vanilla) es invulnerable por defecto. Cuando lanza su ataque de área (`Wall of Storms` vanilla, deja un `Hazard` real —`ShockBarrierHazard`— en el suelo que aplica el Magic Effect `HazardShockDamageFFContact` mientras el jugador esté dentro), si el jugador activa el Activator central **mientras tiene ese efecto encima**, el atronach queda vulnerable e inmóvil (`EnableAI(false)`) durante una ventana en la que solo se le puede quitar hasta 1/3 de su vida máxima antes de volver a ser invulnerable. Repetir el ciclo hasta matarlo. Al morir, el Activator reproduce su animación `open`→`openedloop` nativa; la siguiente activación da el material y cierra el objective 13.

**Decisiones de diseño de esta escena, ya verificadas en el juego**:
- Detectar "el jugador tiene el efecto del hazard" se hace con `HasMagicEffect`, no con eventos de `ActiveMagicEffect` — se probaron `OnEffectStart` sobre un Magic Effect propio (Contact, Fire and Forget) y `RegisterForAnimationEvent` sobre los tres eventos reales de Attack Data del atronach (`attackPowerStart_Forward`/`attackPowerStart_Standing`/`attackStart_Attack_Swipe`), y ninguno de los dos resultó fiable — el primero no disparaba de forma consistente pese a que el daño sí se aplicaba (confirmado viendo bajar la Magicka del jugador en golpes donde el evento no saltaba), el segundo nunca llegó a disparar ni una vez. `HasMagicEffect` sí es fiable porque lee el estado del motor directamente, sin depender de que un callback de script se dispare.
- `Game.FindClosestReferenceOfTypeFromRef` (para buscar el Hazard como referencia) se descartó sin probarse a fondo, por un problema práctico ajeno a la función en sí: la ventana de Properties de la CK para una property de tipo `Form` genérico no deja seleccionar formularios arbitrarios de forma fiable (desplegable limitado a un puñado de formularios internos tipo `PapyrusPersistenceForm`).
- El atronach es un NPC_ dedicado (no el Storm Atronach vanilla) porque `ActorBase.SetInvulnerable`/`SetEssential` afectan a **todas** las referencias que compartan `ActorBase` — usar el vanilla habría hecho invulnerables a todos los Storm Atronach del juego.

## Material 3 — la corteza (objective 12) — PLANIFICADO 2026-09-18 (rediseñado ese mismo día), aún sin construir ni probar en el juego

**Diseño vigente (decisión del usuario; sustituye al primer plan, "la gallina entrega la corteza")**: la **gallina** (confirmado 2026-09-18) bloquea el camino hacia el árbol. El jugador **tiene que hablar con ella sí o sí**:

- **Respuesta correcta** (darle al animal cualquier verdura) → el animal lo deja en paz y el jugador puede pasar.
- **Cualquier otra cosa** — respuesta incorrecta, o cerrar el diálogo con TAB/ESC sin responder — → el animal le lanza directamente un **Fus Ro Dah**.
- **La raíz (antes "corteza") ya no se consigue de un Activator** (cambio del usuario 2026-09-20): es un objeto colocado en el mundo que se recoge como el material 1. Su script (`OnContainerChanged` → mismo camino alternativo del Stage 15) se adjunta a la referencia; el objective 12 muestra target (`RootAlias`) **solo si `GetStageDone(9) == 1`**, mediante una condition en la fila de target del objective (CK Wiki: los targets con condition solo se muestran si se cumple). El texto del objective 12 pasó a "Take the root from the tree that is older than the world itself." **Script (decisión del usuario 2026-09-20)**: se reutiliza el mismo `CAP_ThorMjolnir_MaterialTraker` cambiando properties (`ObjectiveIndex = 12`), en vez de un script hijo; se le añade una property opcional `RoosterRef` (vacía en los materiales 1 y 2) que llama a `Vanish()` de la gallina al recoger la raíz. **Estado 2026-09-20: hecho y verificado en el `.esp`** (MISC, referencia, alias, target condicionado y script con sus properties); **probado en el juego 2026-09-20** (log: pase aplicado con `SetStage(9)`, objective 12 ya mostrado, y `Vanish()` de la gallina al recoger la raíz). Falta confirmar por el usuario el marcador de la raíz tras pagar (caso 3).
- **El estado "prueba superada" va en un `Bool` del script del animal** (persiste en el save por sí solo); no hace falta un stage para eso. ✅ Aclaración 2026-09-19: `Quest.GetStage()` devuelve el **stage completado más alto** (comentario de `Quest.psc` vanilla: "highest completed stage"), no el último puesto; poner un stage bajo después de uno alto no lo hace retroceder. Una suposición mía anterior decía lo contrario y era falsa.
- **Stages de la gallina (planificados, textos pendientes)**: `11` = "no sabe nada" (`!GetStageDone(15) && !GetStageDone(20)`) y `16` = "ya sabe" (mural leído o algún material). 11 < 12 y 16 ∈ [15,25): compatibles con los checks del orbe (`< 12`, `15–24`) y de los materiales (`< 15`) en cualquier orden. Los pone `StartEncounter()` con guarda `!GetStageDone(n)`; `SetStage` sobre una quest parada la arranca. ✅ CK Wiki (vía bellcube, `Quest.SetStage`): *"you can't set the current stage number to a lower value"* (`GetStage()` no retrocede), pero *"this function can still display the journal entry and run script fragments from lower numbered stages, if they hadn't previously been completed"* — así que `SetStage(16)` (y el 9) funcionan aunque el jugador ya esté en el 20 o el 25. La página no dice explícitamente que `GetStageDone` pase a `true`: se confirma con la prueba de consola de más abajo. Además `SetStage` es latente y devuelve `false` si el stage no existe.
- **Objective propio de la gallina (decisión del usuario 2026-09-19)**: los stages 11/16 solo añadían entrada de diario y la quest no se veía en el diario aunque estuviera en marcha (log: `running=TRUE stage=11`). Se añade el objective 16, mostrado por el script en cada encuentro y completado al superar la prueba.
- **Objective 12 al superar la prueba (decisión del usuario 2026-09-20)**: al pagar el tributo se muestra también el objective 12 ("Take bark from the tree…"), aunque el jugador no sepa nada de la quest (stage 0). Antes solo lo mostraban los fragmentos de los stages 15 y 20. Lo hace `ApplyPassedState()` del script de la gallina, tras completar el 16.
- **Objective 10 solo en la primera interacción (decisión del usuario 2026-09-20)**: el trigger del altar (`defaultSetStageTrigSCRIPT`) dispara la primera vez que se pisa sin mirar el progreso; si el jugador ya había cogido materiales, el fragmento del stage 10 mostraba el 10 después de que los stages 15/20 lo hubieran "completado" sin mostrarlo, y quedaba abierto para siempre. Arreglo: `If GetStage() <= 10` en el fragmento del stage 10.
- **Hueco detectado en la prueba real (2026-09-20)**: el pase solo se aplicaba desde el sondeo que arranca el trigger; si el jugador hablaba a mano con la gallina (p. ej. tras un castigo, sin volver a cruzar el trigger), `AcceptTribute` marcaba `bPassed` pero nadie aplicaba el estado (sin stage 9, el objective 16 y su marcador de la gallina quedaban abiertos) y una respuesta incorrecta no se castigaba. Arreglo: `OnActivate` del actor arranca la misma supervisión (`BeginMonitoring()`). ✅ Comprobado en el log 2026-09-20: `OnActivate` sí se dispara al hablar con la gallina (línea "hablas con la gallina fuera del trigger"), con castigo en las respuestas incorrectas y pase aplicado al pagar. **Aclaración del usuario**: en esa prueba usó `tgm` y el empuje no lo sacó del trigger (la gallina es inmóvil por su Race, no se mueve de su sitio). El hueco no es solo un artefacto de la prueba: aparece siempre que el jugador esté dentro del volumen sin "entrar" (empuje corto o bloqueado, guardar y cargar dentro del trigger, viaje rápido que llega dentro). Aplicar el arreglo es opcional.
- **Silencio tras superar la prueba (decisión del usuario 2026-09-19)**: una vez pagado el tributo la gallina no debe ofrecer ninguna opción más. Se descartó `AllowPCDialogue(false)` (flag del actor, no verificado que persista) a favor de condiciones de diálogo sobre el estado de la quest: **stage 9 = "prueba superada"**, puesto por el script tras cerrarse el diálogo, y `GetStageDone(9) == 0` en las tres Infos de entrada (Hello, "I don't have time for this" y "What do you want?"). El 9 es < 12 a propósito: como `GetStage()` es el máximo hecho, un stage entre 12 y 25 rompería la detección de primera visita del orbe (`< 12`) o los checks de materiales (`< 15`). Con las Infos cerradas, "Talk" no abre nada.
- **Arreglo pendiente del Mural**: `OnActivate` mostraba el mensaje solo con `GetStage() < 20`, así que con stage ≥ 20 (p. ej. 25 tras el orbe) no se veía nunca. Decisión del usuario 2026-09-19: mensaje siempre; `SetStage(20)` solo si `GetStage() < 20`.
- **Reintento ilimitado**: tras un castigo el jugador puede volver a entrar al Trigger Box y repetir el encuentro tantas veces como quiera, hasta que consiga la corteza — en ese momento la gallina **desaparece** (`ShallnotPass.Vanish()` → `Disable()`; lo llamará el script del Activator de la corteza; además `StartEncounter()` la hace desaparecer si `IsObjectiveCompleted(12)`, por si la corteza se consiguió por otro camino).
- **Texto de narrador al empezar** (decisión del usuario 2026-09-18): el encuentro abre con un texto de narrador, no una frase de la gallina — "La gallina te mira fijamente. Parece enfadada. No va a dejarte pasar a menos que hagas lo que te pida". Se implementa como **línea Hello** (topic Hello de la pestaña **Misc** de la quest, con el texto entre paréntesis), elegida frente a un popup `Message` (alternativa verificada pero descartada por el usuario).
- **Interpretación mía, confirmar**: una vez superada la prueba (`bPassed`) la gallina deja de iniciar encuentros pero sigue en su sitio hasta que se consiga la corteza.
- **Hallazgo 2026-09-19 (leyendo el `.esp` guardado con un volcado de solo lectura)**: la quest `CAP_ThorMjolnir_Quest_01` **no tiene Start Game Enabled** (byte de flags de `DNAM` = `0x00`; según UESP `0x01` = Start Game Enabled), así que solo arranca cuando algo llama a `SetStage`/`Start` (altar, orb, materiales). Un diálogo de una quest parada no está disponible: es la causa más probable de que la gallina muestre "Talk" y no abra nada (❓ hipótesis, pendiente de reprueba). Solución aplicada: `StartEncounter()` pone el stage 11/16 con `SetStage`, que arranca la quest parada (mismo mecanismo que ya usan el Mural y los materiales). Descartados por el usuario: el `Start()` explícito que hace el vanilla (R4) y marcar Start Game Enabled.
- **Fuerza del empuje (2026-09-19, versión final)**: el lanzamiento del Fus Ro Dah vanilla no depende de la magnitud sino de `PushForce = 15` en el script `VoicePushEffectScript` del efecto Strong. Se probó un `PushActorAway` directo desde el script de la gallina (empujaba pero sin el efecto visual) y se **descartó**: el usuario duplicó el MGEF (`CAP_ThorMjolnir_MagicEffect_VoiceUnrelenting`, con `PushForce = 60` en su script) y lo puso en su hechizo. El script de la gallina solo hace `FusRoDahSpell.Cast(Self, PlayerRef)`.

**Cómo se resuelve cada necesidad** (✅ verificado contra fichero/API real · 📖 fuente de comunidad · ❓ sin verificar, probar en el juego):

| Necesidad | Solución | Estado |
|---|---|---|
| Lanzar Fus Ro Dah sin tenerlo en su lista de hechizos | `Spell.Cast(animal, jugador)` con el hechizo del tercer nivel de Unrelenting Force. Alternativa: `Actor.DoCombatSpellApply(spell, jugador)` | ✅ `Spell.Cast` "funciona aunque `akSource` no tenga el hechizo" (bellcube; instantáneo, no anima al actor, apunta a la cabeza del objetivo, requiere celdas cargadas). UESP: shout `00013E07`, hechizo Fus Ro Dah `00013F3A` (Fus `00013E09`, Fus Ro `00013F39`); los draugr ya lo lanzan contra el jugador. ✅ confirmado en la CK 2026-09-18: Editor ID `VoiceUnrelentingForce3` ("Unrelenting Force - Fus Ro Da"; Type Voice Power, Casting Fire and Forget, Delivery Aimed). Estado final 2026-09-19: hechizo propio con MGEF propio (ver pieza "Spell Fus Ro Dah"), **sin efectos de daño**; ❓ que el efecto empuje al jugador lanzado desde una criatura |
| Forzar que el jugador hable | Trigger Box → `animal.Activate(PlayerRef)` desde script. Plan B: paquete `ForceGreet` (Package Template) en el animal vía alias | ✅ firma de `Activate` (bellcube); ❓ que `Activate` abra el diálogo con una criatura; 📖 ForceGreet: el NPC camina hasta el jugador e inicia el diálogo (campo "ForceGreet Distance") — páginas de origen con 403, no abiertas |
| Que el jugador no pueda salir con TAB | No se intenta bloquear: se detecta y se castiga. Sondeo de `IsInDialogueWithPlayer()` hasta que el diálogo abre y luego hasta que cierra; si no hay `bPassed` → Fus Ro Dah | ✅ función declarada en `ObjectReference.psc` ("Is this actor or talking activator currently talking to the player?"); patrón de sondeo idéntico al vanilla (`DLC2DialogueRRQuestScript`, `BardSongsScript`). 📖 un diálogo totalmente bloqueado exigiría scripting adicional o Scenes (no investigado) |
| Ofrecer "dar verdura" solo si la tiene | Condition `GetItemCount` con una FormList propia `> 0` sobre el jugador (Run On: Reference → `PlayerRef`, o Target) | 📖 con una FormList, `GetItemCount` suma los items de la lista; ❓ nombre exacto del campo "Run On" |
| Consumir la verdura entregada | Recorrer la FormList y `RemoveItem(veg, 1)` de la primera con `GetItemCount > 0` | ✅ `FormList.GetSize/GetAt` y `ObjectReference.GetItemCount/RemoveItem` leídos en los `.psc` vanilla |

**Piezas**:

| Pieza | Notas |
|---|---|
| Voice Type | **No hace falta uno propio para oír el cloqueo**: el Voice Type solo decide en qué carpeta de ficheros de voz busca el motor cada línea; no conozco ningún campo del Topic Info que apunte a un Sound Descriptor vanilla (no verificado en fuente). El cloqueo vanilla se reproduce por script (`Sound.Play`, ver fila del Sound Marker) en el fragmento Begin de cada Info. Un Voice Type propio solo haría falta para colocar ficheros `.fuz` propios ❓ (estructura de carpetas no verificada en esta sesión) |
| `CAP_ThorMjolnir_NPC_Chicken` | NPC_ único con **Race propia** (duplicada de la de la gallina vanilla; el usuario ya la ha creado y tiene marcado **Allow PC Dialogue** en la Race → **no** hace falta `defaultAllowPCDialogueScript`) y **con nombre** (✅ bellcube: el jugador no puede activar una referencia cuya base no tiene nombre, salvo TalkingActivator). Script propio `CAp_ThorMjolnir_ShallnotPass`. Essential/Protected ❓ (que no se pueda matar y romper el gate) |
| ~~`CAP_ThorMjolnir_SM_ChickenCluck`~~ *(omitido por el usuario 2026-09-18: sin cloqueo por ahora, las líneas salen solo como subtítulo; se puede añadir más adelante)* | Sound Marker (Audio → Sound Marker) que referencia un Sound Descriptor vanilla de la gallina (Editor ID sin confirmar: filtrar por "Chicken" en Audio → Sound Descriptor). Trampas ya documentadas en la skill: la property `Sound` pide Sound Marker, no Descriptor; el Descriptor necesita Output Model para oírse en el juego; con `akSource` lejano no se oye (aquí el animal está junto al jugador, así que vale `akSpeakerRef`; si no se oye, pasar `PlayerRef`) |
| `CAP_ThorMjolnir_Trigger_ShallNotPass` | Trigger Box delante del animal; `OnTriggerEnter` (solo jugador) → `ChickenRef.StartEncounter()` |
| `CAP_ThorMjolnir_FormList_Vegetables` (nombre real) | FormList: Cabbage `00064B3F`, Carrot `00064B40`, Gourd `0010D666`, Leek `000669A5`, Potato `00064B41`, Tomato `00064B42`, Ash Yam `0206E7` (Dragonborn) — UESP `Skyrim:Food`. Manzanas fuera (fruta). Cultivos añadidos por otros mods no cuentan salvo que se metan en la lista | ⚠ Contenido real leído del `.esp` (2026-09-19): 10 formas — las 6 verduras (Gourd, Leek, Tomato, Potato, Carrot, Cabbage) **más** `FoodApple`, `FoodApple02`, `FoodBread01A` y `FoodBread01B` (dos manzanas y dos panes, que no son verduras): revisar si es intencionado.
| Diálogo (pestaña Player Dialogue) | **Árbol acordado 2026-09-19** (todas las Infos: `GetIsID` = la gallina sobre Subject, Force Subtitle, sin Has LIP File, sin Say Once). Tras el Hello de narrador hay dos opciones de primer nivel: **(A)** rama ya creada, topic "I don't have time for this" → respuesta de narrador, Goodbye, sin pase → castigo; **(B)** rama nueva, topic "What do you want?" → respuesta: el gallo exige un tributo en forma de verdura o pan (sin Goodbye; su **Link To** apunta a B1 y B2, y la CK muestra solo los que tengan una Info válida) → **B1** "(Offer a tribute)", condition `GetItemCount` de la FormList `>= 1`, Goodbye, fragmento End que llama a `AcceptTribute()` del script de la gallina (consume un elemento y marca el pase); **B2** "I have nothing right now.", condition `GetItemCount` de la FormList `== 0`, Goodbye, sin pase → castigo. 📖 Link To (CK Wiki): si el Info termina y sus topics enlazados tienen Infos válidas se muestra una lista de opciones; si no hay ninguna, se vuelve a la lista de primer nivel. ❓ Run On de la condition de inventario: Reference → `PlayerRef` (o Target), a comprobar |
| Hello (pestaña **Misc** de la quest) | Info del topic Hello con el texto de narrador entre paréntesis. Conditions: `GetIsID` = la gallina `== 1` y `IsInDialogueWithPlayer == 1` (📖 para que solo se diga al abrir una conversación real y no al pasar cerca); Force Subtitle, sin Has LIP File, sin Say Once (se repite en cada reintento), sin Goodbye. ❓ Sin verificar en el juego: cómo se ve el subtítulo (¿nombre de la gallina delante?) y que `Activate` dispare el Hello |
| Spell Fus Ro Dah | property `FusRoDahSpell` → hechizo propio `CAP_ThorMjolnir_Spell_ShallnotPass` (copia del vanilla `VoiceUnrelentingForce3`) con **2 efectos**: MGEF propio `CAP_ThorMjolnir_MagicEffect_VoiceUnrelenting` (copia del Strong; `PushForce = 60`; conserva la condition del keyword de inmunidad) y el Middle vanilla `0007F82E`. **Sin efectos de daño** (se quitaron Generic Damage y Disintegrate) |

Esquema del script del animal (`CAp_ThorMjolnir_ShallnotPass`, extiende `Actor`, adjunto al NPC_ único; `Debug.Trace` según la convención de la skill):

```papyrus
Scriptname CAp_ThorMjolnir_ShallnotPass extends Actor
{Gallina lanza el FushRODa}

Actor Property PlayerRef Auto
Quest Property MyQuest Auto
Spell Property FusRoDahSpell Auto
FormList Property TributeList Auto
Int Property BarkObjectiveIndex = 12 Auto
Int Property RoosterObjectiveIndex = 16 Auto
Int Property PassedStage = 9 Auto
Float Property StartTimeout = 5.0 Auto
Float Property PollInterval = 0.25 Auto

Bool bPassed = false
Bool bRunning = false
Bool bGone = false
Bool bDialogueSeen = false
Float startDeadline

Event OnLoad()
    bRunning = false
    bDialogueSeen = false
    If bPassed
        ApplyPassedState()
    EndIf
EndEvent

Event OnActivate(ObjectReference akActionRef)
    If akActionRef != PlayerRef || bGone || bPassed || bRunning
        Return
    EndIf
    Debug.Trace("[CAP_ThorMjolnir][Gate] hablas con la gallina fuera del trigger: se supervisa igualmente")
    BeginMonitoring()
EndEvent

Function StartEncounter()
    If bGone || bPassed || bRunning
        Return
    EndIf
    If MyQuest.IsObjectiveCompleted(BarkObjectiveIndex)
        Vanish()
        Return
    EndIf
    BeginMonitoring()
    Bool activated = Self.Activate(PlayerRef)
    Debug.Trace("[CAP_ThorMjolnir][Gate] Activate devolvio " + activated)
EndFunction

Function BeginMonitoring()
    bRunning = true
    bDialogueSeen = false
    RegisterEncounterStage()
    startDeadline = Utility.GetCurrentRealTime() + StartTimeout
    Debug.Trace("[CAP_ThorMjolnir][Gate] encuentro iniciado")
    RegisterForSingleUpdate(PollInterval)
EndFunction

Function RegisterEncounterStage()
    If MyQuest.GetStageDone(15) || MyQuest.GetStageDone(20)
        If !MyQuest.GetStageDone(16)
            MyQuest.SetStage(16)
        EndIf
    ElseIf !MyQuest.GetStageDone(11)
        MyQuest.SetStage(11)
    EndIf
    MyQuest.SetObjectiveDisplayed(RoosterObjectiveIndex)
    Debug.Trace("[CAP_ThorMjolnir][Gate] quest running=" + MyQuest.IsRunning() + " stage=" + MyQuest.GetStage())
EndFunction

Function AcceptTribute()
    Bool consumed = false
    Int i = 0
    Int n = TributeList.GetSize()
    While i < n
        Form item = TributeList.GetAt(i)
        If PlayerRef.GetItemCount(item) > 0
            PlayerRef.RemoveItem(item, 1)
            consumed = true
            i = n
        Else
            i += 1
        EndIf
    EndWhile
    If consumed
        bPassed = true
    EndIf
    Debug.Trace("[CAP_ThorMjolnir][Gate] tributo consumido=" + consumed)
EndFunction

Function CompleteRoosterObjective()
    If MyQuest.IsObjectiveDisplayed(RoosterObjectiveIndex) && !MyQuest.IsObjectiveCompleted(RoosterObjectiveIndex)
        MyQuest.SetObjectiveCompleted(RoosterObjectiveIndex)
    EndIf
EndFunction

Function ApplyPassedState()
    If !MyQuest.GetStageDone(PassedStage)
        Bool ok = MyQuest.SetStage(PassedStage)
        Debug.Trace("[CAP_ThorMjolnir][Gate] SetStage(" + PassedStage + ") devolvio " + ok)
    EndIf
    CompleteRoosterObjective()
    Debug.Trace("[CAP_ThorMjolnir][Gate] objective " + BarkObjectiveIndex + " mostrado=" + MyQuest.IsObjectiveDisplayed(BarkObjectiveIndex) + " completado=" + MyQuest.IsObjectiveCompleted(BarkObjectiveIndex))
    If !MyQuest.IsObjectiveDisplayed(BarkObjectiveIndex) && !MyQuest.IsObjectiveCompleted(BarkObjectiveIndex)
        MyQuest.SetObjectiveDisplayed(BarkObjectiveIndex)
    EndIf
EndFunction

Function Vanish()
    bGone = true
    bRunning = false
    CompleteRoosterObjective()
    Debug.Trace("[CAP_ThorMjolnir][Gate] la gallina desaparece")
    Self.Disable()
EndFunction

Event OnUpdate()
    If !bRunning
        Return
    EndIf
    If Self.IsInDialogueWithPlayer()
        bDialogueSeen = true
        RegisterForSingleUpdate(PollInterval)
        Return
    EndIf
    If !bDialogueSeen
        If Utility.GetCurrentRealTime() < startDeadline
            RegisterForSingleUpdate(PollInterval)
            Return
        EndIf
        Debug.Trace("[CAP_ThorMjolnir][Gate] el dialogo nunca llego a abrirse - encuentro abortado, sin castigo")
        bRunning = false
        Return
    EndIf
    bRunning = false
    If bPassed
        Debug.Trace("[CAP_ThorMjolnir][Gate] prueba superada")
        ApplyPassedState()
    Else
        Debug.Trace("[CAP_ThorMjolnir][Gate] castigo: Fus Ro Dah")
        FusRoDahSpell.Cast(Self, PlayerRef)
    EndIf
EndEvent
```

Script del Trigger Box (`CAP_ThorMjolnir_Trigger_ShallNotPass`, extiende `ObjectReference`; mismo patrón que `Trigger_AtronachAtack`):

```papyrus
Scriptname CAP_ThorMjolnir_Trigger_ShallNotPass extends ObjectReference

CAp_ThorMjolnir_ShallnotPass Property ChickenRef Auto
Actor Property PlayerRef Auto

Event OnTriggerEnter(ObjectReference akActionRef)
    If akActionRef == PlayerRef
        ChickenRef.StartEncounter()
    EndIf
EndEvent
```

Fragmento **End** de la Info B1 "(Offer a tribute)": una sola llamada, sin properties propias — la lógica (recorrer la FormList, `RemoveItem`, marcar el pase) vive en `AcceptTribute()` del script de la gallina, cuya property `TributeList` (FormList → `CAP_ThorMjolnir_FormList_Vegetables`) se rellena en la ventana Properties de la gallina. No hay fragmento Begin: se omitió el Sound Marker del cloqueo.

```papyrus
(akSpeakerRef as CAp_ThorMjolnir_ShallnotPass).AcceptTribute()
```

**A verificar en el juego (nada de esto está probado)**: (a) que `Activate(PlayerRef)` abre el diálogo con una criatura — si no, ForceGreet; (b) que la gallina no huye ni deambula fuera del alcance; (c) que el flag Goodbye cierra la conversación tras la respuesta; (d) que `Spell.Cast` del Fus Ro Dah desde la gallina tira al jugador (si no, `DoCombatSpellApply`); (e) que tras un castigo el jugador puede volver a entrar al trigger y repetir; (f) el momento exacto en que `IsInDialogueWithPlayer()` pasa a `true` tras `Activate` (el `StartTimeout` de 5 s es un valor a ojo); (g) que el cloqueo se oye con el Sound Marker y `akSpeakerRef`; (h) que las Infos sin fichero de voz muestran subtítulo.

**Pregunta abierta al usuario**: ¿la gallina debe hacer algo visible al superarse la prueba (apartarse), o basta con que deje de atacar hasta que desaparezca al conseguir la corteza?

## Forja y cierre — diseño 2026-09-20; construida 2026-09-21 (verificada en el `.esp` y **probada en el juego** el mismo día)

Idea del usuario: el pedestal (`CAP_ThorMjolnir_Activator_Forge`, "Pedestal", `REFR 030E48AA`, alias `ForgeALias`, target del objective 17) solo funciona con **los 3 materiales ya en el inventario** (todo o nada). Al depositarlos aparecen en el pedestal; luego el jugador debe lanzar el **grito Storm Call de 3 palabras**; entonces caen rayos del cielo sobre el pedestal (con marcadores en el cielo como origen), hay una explosión y aparece el arma.

**Verificado (Skyrim.esm, .psc vanilla y UESP)**:
- Storm Call: shout `StormCallShout` `0007097D`; palabras `0006029A` (Strun), `0006029B` (Bah), `0006029C` (Qo); hechizos `VoiceStormCall1/2/3` = `00018609`/`0001860A`/`0001860D`; recarga 300/480/600 s. Solo funciona al aire libre. Dónde se aprende cada palabra (UESP): **Strun** en Forelhost (quest "Siege on the Dragon Cult"), **Bah** en High Gate Ruins ("A Scroll For Anska") y **Qo** en Skuldafn Temple. **Skuldafn solo es accesible durante la quest principal "The World-Eater's Eyrie"** (n.º 16 de 18: tras "The Fallen", donde Odahviing te lleva; antes de Sovngarde y Dragonslayer, o sea, tramo final), y **no se puede volver tras entrar en el portal a Sovngarde**: si no se coge la palabra en esa única visita, no se puede conseguir sin consola. Por tanto exigir el nivel 3 = requisito de final de juego con **riesgo real de soft-lock permanente**; los niveles 1 y 2 (Strun / Strun + Bah) se pueden conseguir en cualquier momento. ❓ Según UESP vanilla; no comprobado que el modlist no lo cambie. **Decisión del usuario 2026-09-20: se acepta cualquier nivel** de Storm Call (así el requisito está disponible a mitad de juego, con solo la palabra de Forelhost); el sondeo comprobará los tres efectos de lanzador: nivel 1 `VoiceStormCallEffect1Self` (`000E3F0A`, 60 s), nivel 2 `VoiceStormCallEffect2Self` (`000E3F09`, 120 s) y nivel 3 `VoiceStormCallEffect3Self` (`000D5E81`, 180 s). Consola para probar: `player.teachword` + `player.unlockword` con `0006029A` (Strun), `0006029B` (Bah), `0006029C` (Qo); `tgm` quita el enfriamiento del grito (UESP).
- **Detección del nivel 3**: el hechizo `VoiceStormCall3` aplica al lanzador el efecto `VoiceStormCallEffect3Self` (`MGEF 000D5E81`, duración 180 s; el nivel 2 aplica `000E3F09` y el 1 `000E3F0A`). `Actor.HasMagicEffect(...)` sobre ese efecto sirve para detectar el nivel 3 (mismo método que ya usa el atronach). `OnSpellCast` también detecta shouts pero entrega el shout, no el nivel; no detecta hechizos lanzados por Papyrus.
- **Rayos**: `StormCallLightningBolt03` (`SPEL 000E98A3`, Aimed, efecto `ShockDamageBoltStormAimed`, proyectil `ShockBoltAimStorm` con `Magic\LightningBolt03.nif`); su explosión de impacto (`StormCallImpactSoundExplosion`, `000EAFE3`) no tiene arte, fuerza ni daño. `Spell.Cast(origen, destino)` funciona con un origen que no sea actor (marcador), es instantáneo y exige celdas cargadas.
- Explosión propia ya existente: `CAP_ThorMjolnir_Explosion_MjolnirImpact` (`EXPL 0301BC69`). Arma: `CAP_ThorMjolnir_WeaponActivatorModel` ("Mjolnir") con `CAP_ThorMjolnir_MjolnirPickUp` (al activarlo da `ThorMjolnir` y se borra). Funciones disponibles: `Game.ShakeCamera`, `ImageSpaceModifier.Apply`, `ObjectReference.Enable(bool fade)`, `PlaceAtMe(Form, ...)`, `BlockActivation`, `Game.TeachWord`/`UnlockWord`/`IsWordUnlocked`.

**Flujo propuesto** (stages/objectives por decidir): stage 30 (ya hecho) → el jugador activa el pedestal con los 3 materiales → se retiran del inventario, se activan 3 referencias de exhibición (Static/Activator con el mismo modelo, no MISC, para que no se puedan recoger) → **stage 35** "materiales depositados" (objective 17 completo, objective nuevo "lanza Storm Call") → sondeo de `HasMagicEffect(VoiceStormCallEffect3Self)` con el jugador cerca → secuencia (rayos desde marcadores del cielo con `StormCallLightningBolt03`, sacudida de cámara, explosión propia, exhibición desactivada) → se activa el activator del arma (colocado desactivado) → **stage 40** "arma forjada". ❓ Cierre: objective "coge el arma" y stage 50 al recogerla (requeriría avisar desde `MjolnirPickUp`), o cerrar en el 40.

**Decisiones del usuario 2026-09-20**: se acepta **cualquier nivel** de Storm Call y la quest **se cierra al recoger el arma**. Stages y objectives resultantes: **35** (materiales depositados; completa el objective 17 y muestra el **18** "lanza Storm Call sobre el pedestal"), **40** (arma aparecida; completa el 18 y muestra el **19** "coge el arma"), **50** (arma recogida; completa el 19; marca *Complete Quest*). Los cambios de objectives van en fragmentos de stage (como el del 30) y el script de la forja solo pone stages. El 50 lo pondrá `CAP_ThorMjolnir_MjolnirPickUp` mediante dos properties opcionales nuevas (`MyQuest`, `CompletionStage`), vacías en cualquier otro uso del activator. Piezas del mundo: 3 referencias de exhibición (Static/Activator no recogibles), el activator del arma desactivado (con alias `WeaponAlias` como target del 19) y 2–3 marcadores altos como origen de los rayos.

**Estado 2026-09-20 (verificado en el `.esp`)**: stages 35/40/50 y objectives 18/19 hechos (el 50 con Complete Quest); fragmentos 9/11/13. Piezas del mundo junto al pedestal (`REFR 030E48AA`, ~55531, -52586, 38960): tres Statics de exhibición desactivados (`CAP_ThorMjolnir_Static_Asteroid` `030E7776`, `_Orb` = Stormheart `030E7777`, `_Root` `030E7778`), el activator del arma desactivado (`03007E9D`, alias `MjolnirStaticAlias`, target del objective 19) y **un** XMarker a ~1808 unidades sobre el pedestal (`030E7779`); basta uno si el script lo recoloca con `MoveTo` antes de cada rayo. ⚠ La explosión propia `CAP_ThorMjolnir_Explosion_MjolnirImpact` tiene **fuerza 250, daño 10 y radio 200** (campos leídos del `.esp`): colocada en el pedestal alcanzaría a un jugador que esté a menos de 200 unidades; conviene una copia con daño 0 (y fuerza baja) para la forja. *(A 2026-09-20 faltaban el script de la forja y las properties de `MjolnirPickUp`; ver el estado de 2026-09-21 justo debajo.)*

**Estado 2026-09-21** (verificado leyendo el `.esp` guardado y el `.psc`; compilado limpio con el compilador real, 0 errores / 0 avisos; **probado en el juego el mismo día**, ver "Prueba en el juego" más abajo):

- **`CAP_ThorMjolnir_ForgeScript`** (en `REFR 030E48AA`, 21 properties; el `.esp` guarda 15 — las 6 numéricas usan el valor por defecto del script). Rellenas y comprobadas: `PlayerRef` (jugador), `MyQuest`, `AsteroidItem` = `CAP_ThorMjolnir_Material_AsteroidOre` (`030986CC`), `StormheartItem` = `..._Stormheard` (`030B8994`), `RootItem` = `..._MimameidrRoot` (`030E48A5`), `ShowAsteroid`/`ShowStormheart`/`ShowRoot` = `030E7776`/`030E7777`/`030E7778`, `WeaponRef` = `03007E9D`, `SkyMarker` = `030E7779`, `BoltSpell` = `StormCallLightningBolt01` (`000E4CB7`, mag 40; valen los tres), `StormEffect1/2/3` = `000E3F0A`/`000E3F09`/`000D5E81` (los `...Self`), `FinalExplosion` = `CAP_ThorMjolnir_Explosion_MjolnirImpact`. Errores de rellenado cazados al verificar (corregidos): `AsteroidItem` apuntaba al MISC vanilla `C05FragmentSack` (`000DB351`) y los efectos eran los de área `VoiceStormCallEffect1/2/3` en vez de los `...Self`.
- **Flujo**: `OnActivate` (solo jugador) → con el 40 hecho no hace nada; con el 35 hecho re-arma la vigilancia; sin los 3 materiales: "The pedestal lies dormant."; con ellos: `SetStage(35)` **primero** y solo si queda hecho consume los 3 (`RemoveItem`), activa las 3 exhibiciones y arma la vigilancia. `StartStormWatch()` (pública: la llaman el trigger, el depósito y `OnActivate`) = `UnregisterForUpdate()` + `RegisterForSingleUpdate(PollInterval)`. `OnUpdate` termina la cadena si `bPlaying`, si falta el 35, si ya está el 40, o si el pedestal no tiene 3D o el jugador está a más de `MaxDistance`; si hay Storm Call activo (`HasMagicEffect` de cualquiera de los tres `...Self`) lanza `PlaySequence()` (6 rayos: `SkyMarker.MoveTo` con desvío aleatorio + `BoltSpell.Cast(SkyMarker, Self)` + `ShakeCamera`; rayo final; `PlaceAtMe(FinalExplosion)`; apaga exhibiciones; `WeaponRef.Enable`; `SetStage(40)`); si no, se re-registra. `IsSetupComplete()` valida las 15 properties de objeto y deja un error en el log si falta alguna (sin esa validación, una property vacía perdía los materiales o avanzaba al 40 sin arma). Dos flags: `bBusy` (durante `OnActivate`) y `bPlaying` (durante la secuencia).
- **Sin `OnLoad` ni `OnCellAttach`** (decisión 2026-09-21): `OnLoad` es poco fiable justo tras cargar un save, no salta para referencias desactivadas y salta en cada carga de 3D; `OnCellAttach` no salta en las primeras celdas cargadas desde un save y su documentación no aclara exteriores ni referencias persistentes (el pedestal es persistente, flag `0x400`). La vigilancia se arma por eventos: trigger de la zona, depósito y `OnActivate`.
- **`CAP_ThorMjolnir_Trigger_TOW_001`** (`REFR 0301E21D`, base vanilla `defaultSetStageTRIGPlayerOnly` `00099312`; volumen = primitiva de `bounds` 1020×2011×885, muy inclinada, ~87° en X; su centro está a ~251 uds en horizontal y 1479 **por debajo** del pedestal): ejecuta ahora **`CAP_ThorMjolnir_Trigger_Forge`** (tabla de Scripts). El vanilla `defaultsetStageTrigSCRIPT` (`stage 10`, `doOnce`/`disableWhenDone` = True → se desactivaba tras la 1.ª entrada) queda en la referencia con estado 3 = "Inherited y Removed" (así marca la CK quitar un script heredado de la base; significado según el formato VMAD, no confirmado en el juego). Se propuso el nombre `..._Trigger_TOW`; el usuario lo creó como `..._Trigger_Forge` (código idéntico).
- **Prueba en el juego 2026-09-21** (usuario + `Papyrus.0.log`): el usuario confirma que todo el flujo funciona. El log muestra `01:10:13` `materiales depositados` → `01:11:33` `tormenta detectada: empieza la secuencia` → `01:11:44` `secuencia terminada, SetStage(40) devolvio TRUE`, sin errores de `ForgeScript`, `Trigger_Forge` ni `MjolnirPickUp`. Con eso queda comprobado en el log que se ejecutaron el depósito (incluido `RemoveItem` sobre Quest Objects), la detección con los `...Self`, la secuencia con `Spell.Cast` desde el XMarker y el `SetStage(40)`; el resto (aspecto de rayos y explosión, aparición del arma, cierre en el 50) se apoya en la confirmación del usuario — el 50 no deja línea en el log porque `MjolnirPickUp` no lleva `Debug.Trace`. Observación: la secuencia tardó ~11 s de reloj frente a ~3,4 s teóricos (6 × 0,5 s + 0,4 s): `Utility.Wait` no es preciso y `SetStage(40)` es latente; si se quiere más seca, bajar `StrikeInterval`/`StrikeCount` (properties con valor por defecto en el script, no guardadas en el `.esp`).
- **Sigue abierto**: (a) que la cadena de `OnUpdate` sobreviva a guardar/cargar (no probado); (b) si la explosión propia daña al jugador (daño 10 / fuerza 250 / radio 200 en el `.esp`; el usuario no lo ha comentado) → duplicarla con daño 0 si pasa; (c) si el pedestal cae dentro de la primitiva del trigger: el flujo funcionó, pero el depósito y `OnActivate` también arman la vigilancia, así que la prueba no lo distingue; (d) los `.pex`/`.psc` de `ForgeScript` y `Trigger_Forge` siguen en `overwrite\Scripts` (MO2), sin mover al mod; (e) en saves donde el vanilla ya disparó, el trigger quedó desactivado.
- **Hecho 2026-09-21**: `CAP_ThorMjolnir_MjolnirPickUp` con `MyQuest`/`CompletionStage` = 50 rellenadas solo en la referencia `03007E9D` (verificado en el `.esp`; la base del activator no las lleva). Con esto el flujo completo 30 → 35 → 40 → 50 queda montado y **listo para probarlo en el juego**.
- **Pendiente**: quitar el `OnLoad` de la gallina (`CAp_ThorMjolnir_ShallnotPass`: contador de ciclos en vez de `startDeadline` y reparar el estado en `OnActivate`); limpiezas ya anotadas (huérfanos, `AtronachSpellDetector`, `Debug.Trace` de diagnóstico).

## Escenas de ambientación (no forman parte del flujo de stages/objectives de materiales)

- **Jormungandr** (`CAP_ThorMjolnir_Activator_Jormungandr`): activator decorativo (criatura sin package/AI) que se acerca al jugador al entrar en un Trigger Box y se para en un `XMarkerHeading`. Script `CAP_JorumnandrAlertTrigger` en el trigger. Detalle completo (incl. el debugging de por qué no se movía — Border Region) en `.claude/skills/creation-kit-quest-design/references/quest-mechanics-ck-wiki.md`.

## Decisiones de diseño a respetar

- Los 3 materiales **nunca** llevan marcador de compass — es deliberado, no un olvido.
- Los materiales son Quest Object (no se pueden tirar/vender) siempre vía Alias + checkbox Quest Object. El mecanismo de relleno de la Alias varía según si el material existe de antemano como referencia colocada en el mundo (material 1, Specific Reference fijado a mano en el editor) o se genera en tiempo real (material 2, `ReferenceAlias.ForceRefTo()` desde un script `OnActivate` + `PlaceAtMe` + `AddItem`, porque no hay ninguna referencia previa que seleccionar al diseñar la quest).
- El libro/pista del NPC experto en tecnología dwemer es **solo narrativo** — no tiene tracking de quest propio, no cambia ningún stage ni objective por sí mismo.
- Camino alternativo (Stage 15) y camino normal (Stage 10→20) deben converger sin duplicar ni deshacer progreso — ya verificado en el diseño (ver tabla de Stages).
- Material 3 (corteza): la gallina guardián obliga al jugador a hablar; darle una verdura lo deja pasar, cualquier otra cosa (o cerrar el diálogo) provoca un Fus Ro Dah, con reintento ilimitado. La gallina desaparece cuando se consigue la corteza, que se coge más adelante de un Activator. La corteza puede obtenerse sin haber pasado por ninguna fase anterior (mismo camino alternativo del Stage 15). Decisión del usuario 2026-09-18, rediseñada ese mismo día. Ver sección "Material 3".
- Forja: la vigilancia de Storm Call se arma por **eventos** (trigger de la zona `TOW_001`, depósito, `OnActivate`), sin `OnLoad`/`OnCellAttach`, y el trigger de la zona **no se desactiva nunca** (lo reutiliza el script de la forja). Decisión 2026-09-21, ver sección "Forja y cierre".
