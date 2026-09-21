Extracto verificado (2026-09-20) de guías, hilos y artículos de la **comunidad de modding de Skyrim**
sobre buenas prácticas de código Papyrus, contrastado con el compilador y los `.psc` vanilla de este
PC. Complementa `papyrus-anti-patterns-fireundubh.md` (que trata la wiki de fireundubh). Pensado
para consultarse al escribir o revisar cualquier script de la quest.

## Leyenda de confianza (cada afirmación lleva una marca)

- ✅ **Verificado aquí**: compilando un script de prueba, leyendo su bytecode (`-keepasm`) o leyendo
  el `.psc` vanilla en `D:\Steam\...\Data\Scripts\Source`.
- 📖 **CK Wiki leída**: texto de la CK Wiki (CC BY-SA) obtenido a través de `papyrus.bellcube.dev`
  (accesible; `ck.uesp.net` no lo es, ver más abajo).
- 🗣️ **Comunidad**: afirmación de una persona concreta (se indica quién y cuándo); útil pero no
  autoritativa.
- ⚠️ **Sin verificar**: solo se vio un resumen/snippet de buscador, o es una inferencia mía.
  No tratarlo como hecho.

## Fuentes consultadas y si son accesibles (no repetir intentos fallidos)

| Fuente | Estado (2026-09-20) |
|---|---|
| `dennissoemers.github.io` (artículo de micro-optimización, 2022) | ✅ `curl` normal |
| `nexusmods.com/.../articles/<id>` (artículos) y `forums.nexusmods.com` | ✅ `curl` con user-agent de navegador; ~200 KB de menús por página: recortar anclando en el título / `## About this mod` |
| `raw.githubusercontent.com`, `api.github.com` | ✅ |
| `tesalliance.org`, `afkmods.com`, `thallassathoughts.wordpress.com`, `steamcommunity.com/groups/SkyrimCKPublic` | ✅ `curl` |
| `papyrus.bellcube.dev` | ✅ `WebFetch` (texto de la CK Wiki con su cita) |
| `ck.uesp.net` **y** `skyrimck.uesp.net` | ❌ `403` (Cloudflare), también con `?action=raw` |
| `creationkit.com` | ❌ devuelve una página de mantenimiento de XWiki, no el contenido |
| `web.archive.org` | ❌ `429` "suspected abusive bot traffic" (no reintentar en bucle) |
| `wiki.beyondskyrim.org` | ❌ `403` (ya documentado) |
| `gamesas.com` (foros de Bethesda) | ❌ el dominio ya no sirve a Bethesda: `curl` no conecta y `WebFetch` recibe un certificado de **apple.com** |

Consecuencia: las notas de la CK Wiki que **no** tienen página de función en bellcube (p. ej.
*Threading Notes*, *Save Files Notes*) solo se conocen aquí por **snippets de buscador** (⚠️).

## 1. El motor: presupuesto de tiempo y "stack dumps"

- 🗣️ Soemers (2022): el motor da a Papyrus un **presupuesto fijo de tiempo por frame** en el hilo
  principal, por defecto **1,2 ms** (Thallassa, 2016, lo confirma como `fUpdateBudgetMS=1.2` /
  `fExtraTaskletBudgetMS=1.2`), compartido por todos los scripts. Consecuencia doble: un script
  caro **no baja el FPS**, pero **retrasa a los demás scripts** (incluidos los tuyos). Una carga alta
  y sostenida puede "hacer bola de nieve".
- 🗣️ Soemers (hipótesis, "parece que"): en SSE hay además un presupuesto **por script y por frame**,
  medido en **instrucciones** y no en tiempo. Su nota al pie: *Papyrus Tweaks NG* (Nightfallstorm)
  puede subir las operaciones por frame y por pila, lo que resta importancia a las micro-
  optimizaciones. Ni él ni Thallassa recomiendan tocar esos valores del `.ini`.
- 🗣️ Nightfallstorm (autor de Papyrus Tweaks NG, Nexus, 2022): un **stack dump** ocurre cuando el
  motor está sobrecargado demasiado tiempo (**5 s por defecto**), o al escribir `DumpPapyrusStacks`
  en la consola; imprime todas las pilas en el log. Es una operación **de solo lectura**: no daña
  scripts ni saves. Puede ser síntoma de una lista de mods "script-heavy". (Un comentarista en 2024
  reporta congelaciones de ~30 s al volcar; no vi respuesta del autor.)
- 🗣️ Cita atribuida a *SmkViper* (ya en `papyrus-anti-patterns-fireundubh.md`): >100 hilos vivos a la
  vez durante unos segundos y Papyrus empieza a volcar pilas.

## 2. Micro-optimizaciones — verificadas en bytecode

Fuente: Soemers, *11 Micro-optimisation Tips for Skyrim's Scripting Language* (2022). Los puntos 1-6
los he **reproducido compilando** y leyendo el `.pas`; los 7-11 no.

| Patrón | Bytecode real en Skyrim SE ✅ | Preferir |
|---|---|---|
| `Utility.Wait(1)` | `CAST ::temp 1` **antes** de `CALLSTATIC Wait` (cast en ejecución) | `Utility.Wait(1.0)` — sin `CAST` |
| `If f > 1` (`f` Float) | `CAST` + `COMPAREGT` | `If f > 1.0` |
| `If b == true` | `COMPAREEQ` + `JUMPF` | `If b` — solo `JUMPF` |
| `If b != true` | `COMPAREEQ` + `NOT` + `JUMPF` | `If !b` (`NOT` + `JUMPF`) |
| `Game.GetPlayer()` | `CALLSTATIC game GetPlayer` extra | `Actor Property PlayerREF Auto` (Auto-Fill) |
| Funciones abreviadas de `Actor` | `GetAV`, `GetAVPercentage`, `GetBaseAV`, `DamageAV`, `ModAV`, `SetAV`, `ForceAV`, `RestoreAV` son funciones **Papyrus de una línea** que llaman a la larga (`Actor.psc` vanilla), no nativas | la versión larga: `GetActorValue`, `DamageActorValue`… |
| Funciones "de conveniencia" | `GlobalVariable.GetValueInt()` = `GetValue() as int`; `ReferenceAlias.GetActorReference()` = `GetReference() as Actor`; `GetRef()` → `GetReference()`; `GetActorRef()` → `GetActorReference()` (**wrapper de wrapper**) — todas en `.psc` vanilla | `GetValue() as Int`, `GetReference() as Actor` |

Otros consejos del artículo, **sin probar aquí** (🗣️): asignar directamente la expresión booleana en
vez de `If cond a = True Else a = False`; no comprobar en un `ElseIf` lo que el `If` ya descartó;
anidar (`If a` → `If b`/`ElseIf c`) en vez de repetir `a &&` en cada rama; `x + Utility.RandomInt(5, 20)`
en vez de `Utility.RandomInt(x + 5, x + 20)`. También: no pasar un `Actor` a una función que pide
`ObjectReference` si se hace muchas veces (cast implícito).

**Matiz honesto, para no sobreaplicar**: el propio código vanilla usa `GetActorRef()` y
`Game.GetPlayer()` (visto en fragments `QF_*`/`TIF_*` vanilla ✅). Estos ahorros valen en **rutas
calientes** (bucles, `OnUpdate`, eventos que saltan a menudo). Los cuatro primeros de la tabla
(`Wait(1.0)`, `f > 1.0`, `If b`, `PlayerREF`) no cuestan legibilidad y conviene aplicarlos siempre; los
*wrappers* de conveniencia, solo donde importe. Soemers defiende además que optimizar scripts que
"no tienen prisa" reduce el daño a los que sí la tienen, y desaconseja depender solo de perfilar
(los scripts reales duran microsegundos).

## 3. Eventos, updates y esperas

- 📖 **`RegisterForSingleUpdate(afInterval)`**: no hace falta `UnregisterForUpdate()` salvo para
  cancelar antes de tiempo. El intervalo **ignora el tiempo en menús**. *"Aliases and quests will
  automatically unregister for this event when the quest stops."* El evento solo llega al objeto que
  se registró, no se reenvía a aliases/efectos adjuntos.
- 📖 **Trampa de re-registro**: *"You should call UnregisterForUpdate() before calling this function a
  second time prior to the timer elapsing or before calling RegisterForUpdate(...), or strange
  behavior may result."* Llamar `RegisterForUpdate` tras `RegisterForSingleUpdate` *"will replace the
  single update's timer interval"*; llamadas repetidas sin `UnregisterForUpdate` *"do not behave as
  expected, if the timer hasn't elapsed"*. (Un resumen de buscador decía que la segunda llamada
  simplemente sustituye a la primera: **seguir la página primaria**, desregistrar antes.)
- 📖 **`RegisterForUpdate`**: *"The interval is only counted when the game is not in menu mode."*
  Mismo aviso de desregistrar antes de volver a registrar.
- ⚠️ CK Wiki *Save Files Notes* (snippet): al recibir `OnUpdate` continuo, el siguiente evento puede
  llegar **antes de que termine el anterior**, lo que puede causar *save bloat*. → intervalos
  generosos, y con `RegisterForSingleUpdate` re-registrar **al final** del manejador.
- 📖 **`Utility.Wait(afSeconds)`**: función **latente**, tiempo **real**; *"Wait time isn't precise,
  based on framerate and general Papyrus system workload"*; no avanza con un menú abierto (existe
  `WaitMenuMode`; y `WaitGameTime`); `Wait(0)` vuelve de inmediato. Consejo repetido en varias fuentes
  (🗣️ Beyond Skyrim ya citada en `SKILL.md`; resumen de buscador de un hilo de Bethesda ⚠️):
  evitar `Wait` largos y preferir `RegisterForSingleUpdate`/`OnUpdate`; evitar el sondeo con
  intervalos cortos cuando exista un **evento** que detecte lo mismo.
- 📖 **`AddInventoryEventFilter(Form akFilter)`**: el filtro se aplica *"only to the specific
  Reference, Alias, or Active Magic Effect it is added to"*; los filtros **se acumulan** (para
  sustituirlos hay que quitar los anteriores); una `FormList` filtra por sus formularios pero **no
  entra en listas anidadas**; y *"Skyrim does not support passing None to this function; that was
  added in Fallout 4. You can get identical behavior by adding an empty FormList."* ✅ `AddInventoryEventFilter(None)`
  **compila sin aviso** en Skyrim, así que no hay red de seguridad en compilación (y el ejemplo de la
  última página de la wiki de fireundubh lo usa: es de Fallout 4). 🗣️ Un usuario de un foro de Bethesda
  (fecha no vista) opina que el filtro nativo será más rápido que filtrar en Papyrus — plausible, no medido.

## 4. Hilos, bloqueo y estados

- ⚠️ CK Wiki *Threading Notes* (snippet): solo **un hilo a la vez** puede operar sobre una instancia
  de script; al activarse *"locks"* el script; cualquier llamada **externa** (nativa o no, latente o
  no) puede **liberar** el bloqueo; una llamada es "interna" solo si pertenece a la misma instancia
  o a un ancestro.
- 🗣️ Artículo de Nexus *Profiling and Papyrus* (autor del *Papyrus Profiling Parser*): coincide —
  *"only one thread can operate on a script at a time"*; las funciones muy llamadas hacen cola y
  cada llamada suya las ralentiza para los demás.
- ⚠️ Un tutorial de comunidad (resumen de buscador de un hilo de Bethesda / Cipscis) afirma que un
  `Bool` usado como guarda contra reentrada introduce una condición de carrera, mientras que un
  **estado** (`GoToState`) la evita. Plausible dado el bloqueo anterior; **no verificado**.

## 5. Guardado y persistencia

- ⚠️ CK Wiki *Save Files Notes* (snippet): quitar una property o variable **después** de que exista
  un save deja su valor inaccesible para los scripts activos y escribe un aviso en el log; se limpia
  en el siguiente guardado. Al desinstalar un mod, los scripts ya adjuntos permanecen en el save y el
  objeto sigue registrado para los eventos que ya tenía, aunque el `.pex` desaparezca.
  *Inferencia mía*: al iterar sobre esta quest, un cambio estructural (renombrar/quitar properties) en
  un script ya presente en un save puede dejar avisos en el log — probar en un save previo a ese script.
- 🗣️ Beyond Skyrim (ya en `SKILL.md`): una property de `ObjectReference`/`Actor` hace esa referencia
  **persistente**; preferir Aliases.

## 6. Ciclo de vida de los scripts de quest

- 📖 (ya en `quest-mechanics-ck-wiki.md`) *"If a 'Start Game Enabled' quest is not also flagged to
  'Run Once', its OnInit event will fire twice."* No aplica a una quest arrancada por Trigger Box.
- 🗣️ Gist de *requinix* (sin fecha, único autor): **nueva partida**: variables de quest y estado
  Papyrus fijados; SEQ arrancadas → `OnInit` #1 (todas las quests) → `OnInit` #2 (SEQ no *run-once*).
  **Cargar partida**: estado restaurado → `OnGameReload` para las quests que estaban en marcha;
  *"OnGameReload requires SkyUI's SKI_PlayerLoadGameAlias script; or use Player's OnPlayerLoadGame
  event"*.
- ✅ `OnPlayerLoadGame` **está declarado en `ReferenceAlias.psc` y `Actor.psc` vanilla, y no en
  `Quest.psc`**. Un script `extends Quest` con `Event OnPlayerLoadGame()` **compila sin ningún aviso**
  (probado), pero 🗣️ la comunidad dice que **nunca se dispara** desde un Quest script.
  🗣️/⚠️ Patrón de comunidad (snippet, no probado en el juego): el Quest script usa `OnInit` para la
  primera vez y llama a una función de mantenimiento; un **ReferenceAlias del jugador** (script
  `extends ReferenceAlias`) implementa `OnPlayerLoadGame` y llama a la misma función; una property
  `Float`/`Int` de **versión** en el quest decide qué migrar tras actualizar el script.
- 🗣️ Un hilo de Nexus (2024, un solo usuario) afirma que el `OnInit` de un quest script "salta varias
  veces, una por cada cambio de stage". **Descartado**: contradice la CK Wiki (dos veces solo en
  Start Game Enabled sin Run Once) y el gist; probablemente confusión con `OnStageChange`/fragments.
- 📖 **Archivos SEQ** (texto de *Quest Data Tab* citado en la página Nexus de un SEQ para EFF):
  *"As of 1.7 the game uses SEQ files in order to track Start-Game Enabled quests you've added, and
  possibly ones you've altered. These files are needed for your dialogue and scenes to work
  properly. You may generate them using TES5Edit or the CreationKit."* ⚠️ Regenerar el `.seq` cada vez
  que se añade o quita una quest Start Game Enabled. **Esta quest arranca por Trigger Box (no SGE), así
  que no lo necesita tal como está diseñada**; solo importa si algún día se marca SGE.

## 7. Aliases desde Papyrus

- ✅ `ReferenceAlias` **no tiene `Enable()`**: `Al.Enable()` → `Enable is not a function or does not
  exist` (mismo mensaje que reporta un hilo de TES Alliance, 2015). Usar `Al.GetReference().Enable()`
  (la propia `GetReference()` devuelve `ObjectReference`). Para un `Actor`, `GetReference() as Actor`
  (ver tabla de la sección 2 sobre por qué es mejor que `GetActorReference()`).
- ✅ Los fragments de quest generan las properties de alias como **`Alias_<NombreExactoDelAlias>`**
  (`QF_*` vanilla). 🗣️ Steam CK group (2020): un fragment que no compilaba con `variable Alias_Heirloom
  is undefined` se debía a un **nombre de alias mal escrito** → el nombre debe coincidir exacto con el
  de la pestaña Aliases.
- 🗣️ Nexus (2024, un usuario): en el `OnInit` de un script de `ReferenceAlias` con *Find Matching
  Reference / In Loaded Area*, `GetReference()` devolvió `None`. Observación de un caso, **no
  generalizable** a Specific Reference (el caso de esta quest). Regla de bajo coste: comprobar `None`
  antes de usar el resultado.

## 8. Fragments

- ✅ Estructura real (vanilla `QF_*` y `TIF_*`): la CK genera `Scriptname QF_… Extends Quest Hidden`
  con el aviso *"Do not edit anything between this and the end comment"*; las properties
  `Alias_<Nombre>`; y cada `Function Fragment_N()` empieza con un bloque
  `;BEGIN AUTOCAST TYPE <TuScriptDeQuest>` que declara `kmyQuest` **ya tipado como tu script de
  quest**, así que `kmyQuest.MiFuncion()` funciona. Los de diálogo (`TIF_*`) extienden `TopicInfo` y
  reciben `akSpeakerRef` (con `akSpeaker` como `Actor`).
- ✅ Como un `QF_*` extiende `Quest`, `SetStage`/`SetObjectiveDisplayed` se llaman directamente; en un
  fragment de **diálogo** hace falta `GetOwningQuest()` (🗣️ Altbert, Steam CK group 2020, pide precisar
  siempre si el código está en un fragment de quest, de diálogo o en el script de la quest, porque la
  sintaxis cambia; el cast `(GetOwningQuest() as MiScript).Foo()` es el patrón de comunidad).
- ⚠️ *Inferencia mía, coherente con la práctica ya seguida en esta quest*: dejar el fragment en **una
  sola llamada** a una función del script de la quest, con la lógica y los `Debug.Trace` en el script
  (más fácil de editar, versionar y depurar que el bloque generado por la CK).

## 9. Depuración y perfilado

- ✅ Firmas en `Debug.psc` vanilla: `Trace(string asTextToPrint, int aiSeverity = 0)`, `TraceStack(...)`
  (añade la pila de llamadas), `TraceUser(asUserLog, asTextToPrint, aiSeverity)` (log de usuario
  propio), `TraceAndBox(...)`, y `TraceConditional(...)` (esta última **no es nativa**: es una función
  Papyrus, con su coste de llamada). ⚠️ Severidad `0` info / `1` aviso / `2` error (resumen de la CK Wiki).
- ✅ Perfilado: `bEnableProfiling=1` en `[Papyrus]`; `Debug.StartScriptProfiling("NombreDelScript")` /
  `StopScriptProfiling`, y `Debug.StartStackProfiling()` / `StopStackProfiling()`; el comentario del
  propio `Debug.psc` avisa de que no hacen nada si `bEnableProfiling` está apagado. 🗣️ Artículo de
  Nexus *Profiling and Papyrus*: `StartScriptProfiling` se llama **desde otro script** distinto del
  perfilado; da tiempos de cada función/evento (estados `QUEUE_PUSH`/`PUSH`/`QUEUE_POP`/`POP`); el
  perfilado de pila (`StartStackProfiling`, dentro de la función) desglosa qué llamadas la ralentizan
  (*"external function calls are generally what makes a function slow in Papyrus"*); los logs van a
  `Documents\My Games\Skyrim Special Edition\Logs\Script\Profiling`; **quitar las llamadas de
  perfilado antes de distribuir**. Herramientas citadas: *Papyrus Profiling Parser* (Nexus 39107) y
  *speedscope* (visor de flamegraphs).
- 🗣️ Nexus *Debugging* (Nexus, artículo de un mod): `bEnableLogging=1`, `bEnableTrace=1`,
  `bLoadDebugInformation=1`, `bEnableProfiling=1` en `[Papyrus]`, y que el logging **puede afectar al
  rendimiento: desactivarlo cuando no haga falta**. (Las tres primeras ya están en `SKILL.md`.)

## 10. Nombres y estilo

- 🗣️ fireundubh (AFK Mods, 2016, con ayuda de *SmkViper*, que él cree que lidera la programación de
  Papyrus en Bethesda): prefijos **de parámetros**: `a` = argumento, más `k` objeto, `b` bool, `i` int,
  `f` float, `s` string (`p`/`r` existen en el código interno pero **no deben usarse**). Y literalmente:
  *"There doesn't appear to be any conformity to conventions with regard to local and script
  variables."* Tendencias (no reglas): properties y variables de script en UpperCamelCase, locales en
  lowerCamelCase, properties de ReferenceAlias con prefijo `Alias_`; 🗣️ (Kesta, mismo hilo) constantes
  de solo lectura en `MAYUSCULAS_CON_GUION_BAJO`. → **Solo los parámetros tienen convención firme.**
- 🗣️ `rwebster85/PapyrusCodingStandards` (GitHub, feb-2023, 0 estrellas, sin licencia): una
  especificación **personal**, no un estándar de comunidad. Propone: palabras clave en minúscula,
  4 espacios, líneas ≤120, orden de cabecera (declaración → imports → properties → variables de
  script), eventos de inicialización primero, PascalCase para objetos y `snake_case` para el resto, y
  continuación de línea con `\`. Útil solo si el proyecto decide adoptarla **entera y de forma
  consistente** (su regla de casing choca con la observación de fireundubh); no aplicar a medias.

## 11. Herramientas de análisis (opcionales, sin instalar ni probar aquí)

- ⚠️ **Papyrus Linter** (Nexus 189862, Idrinth, MIT, v1.45.0 actualizada el 2026-09-20): *"Catch bugs in
  your papyrus scripts the Creation Kit's compiler lets through"*; standalone, VS Code y Sublime Text.
  Su changelog menciona un lint `none-form-usage`. La página lleva las etiquetas **"AI-Generated
  Content"**; no lo recomiendo sin probarlo. Encaja con lo observado (el compilador de Skyrim deja
  pasar una ruta sin `Return` o un `AddInventoryEventFilter(None)`).
- ⚠️ `joelday/papyrus-lang` (extensión de VS Code; descripción de GitHub: completado, ir a la
  definición, diagnósticos en vivo). *Champollion* (decompilador) se cita en el artículo de Soemers
  para leer bytecode; aquí basta con `-keepasm`.

## 12. Discrepancias entre fuentes (cómo se resolvieron)

| Tema | Fuentes | Resolución |
|---|---|---|
| Dividir `||` en varios `If` vs devolver la expresión `Bool` directa | fireundubh (dos páginas que se contradicen) | ver `papyrus-anti-patterns-fireundubh.md` (decisión propia) |
| Funciones cortas de conveniencia | fireundubh/legibilidad vs Soemers/rendimiento | legibilidad por defecto; evitar wrappers solo en rutas calientes |
| Segunda llamada a `RegisterForSingleUpdate` | resumen de buscador ("sustituye") vs 📖 CK Wiki ("comportamiento extraño") | seguir la CK Wiki: `UnregisterForUpdate()` antes |
| `OnInit` "una vez por stage" | un usuario de Nexus vs 📖 CK Wiki + gist | descartado |
| `None` en `AddInventoryEventFilter` | wiki fireundubh (FO4) vs 📖 CK Wiki (Skyrim no lo soporta) | evitar; usar `FormList` vacía |

## 13. Lo que sigue abierto (no afirmar sin comprobarlo)

- Texto completo de *Threading Notes* y *Save Files Notes* (solo snippets; `ck.uesp.net` bloqueado).
- Qué devuelve la VM exactamente cuando una función tipada termina sin `Return` (compila sin aviso; el
  comportamiento en ejecución no se probó).
- Si el presupuesto por script/frame en instrucciones es real o una hipótesis de Soemers.
- Si un `Bool` como guarda de reentrada es de verdad peor que un estado.
- El patrón `OnInit` + ReferenceAlias del jugador con `OnPlayerLoadGame` en el juego real.
