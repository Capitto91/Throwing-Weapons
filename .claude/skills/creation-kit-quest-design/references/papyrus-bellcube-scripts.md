Extracto verificado (2026-08-31) de **`https://papyrus.bellcube.dev/`** ("The Papyrus Index"),
aportado por el usuario como fuente adicional para buscar scripts/funciones/eventos vanilla de
Papyrus reutilizables en esta quest.

## Qué es y por qué es una fuente valiosa

- Es un índice de referencia de Papyrus (scripts, funciones, eventos, propiedades y structs) que
  agrega contenido del **CK Wiki** (`ck.uesp.net`) y de extensiones xSE (SKSE y librerías de la
  comunidad), organizado por juego (Skyrim SE, Fallout 4, Fallout 76, Starfield).
- **A diferencia de `ck.uesp.net`, este dominio SÍ es fetchable directo** (`WebFetch` normal, sin
  el bloqueo de Cloudflare documentado en `quest-mechanics-ck-wiki.md`) — confirmado en esta
  sesión sobre varias páginas distintas. Es, en la práctica, la forma más rápida y fiable de
  conseguir la firma exacta de una función/evento de Papyrus sin depender de snippets de
  `WebSearch`.
- Cada página de función/evento individual **cita su fuente** explícitamente: el CK Wiki bajo
  licencia **Creative Commons Attribution-ShareAlike** (misma licencia que ya se documentaba para
  `ck.uesp.net`), a veces con enlace directo a la página original (p. ej.
  `https://ck.uesp.net/wiki/GetItemCount_-_ObjectReference`). Esto lo hace citable con el mismo
  nivel de confianza que el propio CK Wiki, solo que accesible.
- **Patrón de URL confirmado**: `https://papyrus.bellcube.dev/<juego>/script/<scriptname>/` para
  la página de un script (lista sus funciones/eventos/propiedades), y
  `.../script/<scriptname>/function/<nombre>/` o `.../event/<nombre>/` para el detalle de cada
  miembro. Slug del juego para Skyrim SE: **`skyrimse`** (confirmado vía `sitemap.xml` real del
  sitio, no adivinado — `fallout4`, `fallout76`, `starfield` son los otros tres).
- El sitio tiene su propio `sitemap.xml` (en realidad un índice de 3 sitemaps,
  `sitemap-0/1/2.xml`, ~10800 URLs en total) — útil para descubrir de golpe qué scripts/miembros
  están indexados sin tener que adivinar nombres uno a uno.

## Catálogo de scripts relevantes para esta quest (URLs confirmadas en el sitemap)

No es la lista completa de cada script (el sitio indexa cientos, incluida gran cantidad de
librerías de mods vía SKSE) — son los que importan para el diseño de la quest de recolección.
Página base de cada uno: `https://papyrus.bellcube.dev/skyrimse/script/<nombre>/`.

- **`quest`** — funciones: `start`, `stop`, `reset`, `getstage`, `setstage`,
  `getcurrentstageid`, `setcurrentstageid`, `isrunning`, `isactive`, `iscompleted`,
  `setobjectivedisplayed`, `setobjectivecompleted`, `setobjectivefailed`,
  `isobjectivedisplayed`/`completed`/`failed`, `getalias`, `getaliasbyid`, `getaliasbyname`,
  `getnumaliases`, `completequest`, `completeallobjectives`, `failallobjectives`, `getpriority`.
  Eventos: toda la familia `OnStory*` (`OnStoryChangeLocation`, `OnStoryKillActor`,
  `OnStoryCraftItem`, `OnStoryDialogue`, etc. — 24 en total, ver detalle de
  `OnStoryChangeLocation` más abajo).
- **`objectreference`** — funciones relevantes: `getitemcount`, `additem`, `placeatme`,
  `placeactoratme`, `moveto`, `getcurrentlocation`, `isinlocation`, `getdistance`,
  `getnumreferencealiases`, `getreferencealiases`. Eventos relevantes: `ontriggerenter`,
  `ontriggerleave`, `ontrigger`, `onitemadded`, `onitemremoved`, `onread`, `onactivate`,
  `oncontainerchanged`.
- **`referencealias`** — mismo set de eventos que `objectreference` (hereda el comportamiento,
  incluye `onread`, `ontriggerenter`, `onitemadded`). Funciones propias del alias:
  `getreference`/`getref`, `forcerefto`, `forcerefifempty`, `clear`, `trytoclear`,
  `trytoenable`/`trytodisable`, `trytomoveto`, `trytoreset`, `addinventoryeventfilter`.
- **`book`** — funciones: `getskill`, `getspell`, `isread`, `istakeable`. **No define su propio
  `OnRead`** — lo hereda de `objectreference` (ver detalle abajo), así que un script que extienda
  `ObjectReference` sobre la referencia del libro ya recibe el evento.
- **`constructibleobject`** — 11 funciones: `getresult`/`setresult`,
  `getresultquantity`/`setresultquantity`, `getnumingredients`,
  `getnthingredient`/`setnthingredient`, `getnthingredientquantity`/`setnthingredientquantity`,
  `getworkbenchkeyword`/`setworkbenchkeyword`. Según la propia página, **estas funciones requieren
  SKSE** para funcionar (el script `ConstructibleObject.psc` vanilla no las expone) — no
  verificado de forma cruzada contra otra fuente, tratarlo como dato de esta única página. Solo
  hace falta si se quiere leer/modificar una receta COBJ *desde Papyrus*; para el flujo normal de
  "duplicar `RecipeWeaponIronSword` y editar campos en el editor" (ya documentado en
  `quest-mechanics-ck-wiki.md`) no hace falta ningún script.
- **`debug`** — `notification` (la función exacta para "notificación en pantalla" que pide el
  hito 2 del usuario), `messagebox`, `trace` y variantes.
- **`message`** — `show`, `showashelpmessage`, `resethelpmessage` — alternativa a
  `Debug.Notification` si se quiere un `Message` form con botones en vez de solo texto.
- **`location`** — `getparent`, `hasreftype`, `iscleared`, `isloaded`, `issamelocation`,
  `getreftypealivecount`/`deadcount` — relevante solo si se acaba optando por la Opción B
  (Story Manager) del disparador del altar.
- **`scene`** — `start`, `forcestart`, `stop`, `isplaying`, `isactioncomplete`,
  `getowningquest` — para el momento de forjar el arma si se decide como Scene coordinada (hito 4).

## Detalle verificado de las funciones/eventos más relevantes para esta quest

Contenido extraído literalmente de cada página vía `WebFetch`, todas citando el CK Wiki
(CC BY-SA) como fuente original.

### `Quest.SetStage(int aiStage)` — bool

Fuente: `https://papyrus.bellcube.dev/skyrimse/script/quest/function/setstage/` (cita
`https://ck.uesp.net/wiki/SetCurrentStageID_-_Quest`).

- Intenta fijar la etapa actual; devuelve `true` si existe y se aplica, `false` si no.
- **Es una llamada latente**: espera a que la quest arranque (si hace falta) y a que terminen
  todos los fragmentos de script de esa etapa antes de devolver el control.
- **Trampa documentada**: aunque no puedes bajar el número de etapa actual con esta función, sí
  puede volver a ejecutar el log entry y los fragmentos de etapas **anteriores** numeradas si no
  se habían completado antes — para retroceder de verdad, usar `Reset()` antes de `SetStage()`.

### `ObjectReference.OnTriggerEnter(ObjectReference akActionRef)`

Fuente: `https://papyrus.bellcube.dev/skyrimse/script/objectreference/event/ontriggerenter/`.

- Confirma lo ya documentado en `quest-mechanics-ck-wiki.md` (Opción A del disparador del altar),
  con matices adicionales:
  - Puede llegar **desordenado** respecto a `OnTriggerLeave` — mejor llevar un contador que un
    booleano si hace falta rastrear "dentro/fuera".
  - No detecta bien actores muertos, ni NPCs cruzando puertas de teletransporte.
  - Si la geometría del trigger se descarga/recarga, el evento puede dispararse varias veces de
    forma inconsistente — mitigación citada: usar `TranslateToRef()` en vez de desplazamientos
    normales para evitar descargas innecesarias.

### `ObjectReference.OnRead()` — el mecanismo real del hito 2

Fuente: `https://papyrus.bellcube.dev/skyrimse/script/objectreference/event/onread/`.

- *"Event received when this object (which is a book) has been read"* — salta cuando se abre la
  interfaz de lectura del libro.
- Sin parámetros. Vive en `ObjectReference` (heredado por `Book`, no hace falta ningún evento
  propio de `Book`).
- **Esto resuelve el hueco que había dejado abierto la skill** para el hito 2 ("el jugador lee un
  objeto → notificación en pantalla"): un script que extienda `ObjectReference` sobre la
  referencia del libro/nota, con `Event OnRead()` llamando a `Debug.Notification(...)` (ver
  siguiente) y/o `MiQuest.SetStage(20)`.

### `Debug.Notification(string asNotificationText)`

Fuente: `https://papyrus.bellcube.dev/skyrimse/script/debug/function/notification/`.

- Muestra el texto en la esquina superior izquierda de la pantalla.
- **Trampa de caracteres documentada**: si el texto lleva `<` y `>` a la vez, todo lo que hay
  entre ellos (símbolos incluidos) no se muestra; solo `<` sin `>` trunca desde ahí en adelante;
  solo `>` sin `<` no tiene problema. Evitar `<`/`>` en el texto de la notificación por seguridad.

### `Quest.OnStoryChangeLocation(ObjectReference akActor, Location akOldLocation, Location akNewLocation)`

Fuente: `https://papyrus.bellcube.dev/skyrimse/script/quest/event/onstorychangelocation/`.

- *"Event called when this quest is started via a change location story manager event."*
- **Matiz nuevo respecto a lo ya documentado en `quest-mechanics-ck-wiki.md` para la Opción B**:
  cuando una quest tiene asignado el Event `Change Location Event` en la pestaña Data, el propio
  **Quest script** recibe este evento directamente (con el actor, la location vieja y la nueva ya
  resueltos como parámetros tipados) — no hace falta leer los datos a través de `GetEventData()`
  sobre una alias. Refuerza que la Opción B, si se usara, integra el dato del evento de forma más
  directa de lo que parecía al leer solo la pestaña Data.

### `ReferenceAlias.ForceRefTo(ObjectReference akNewRef)`

Fuente: `https://papyrus.bellcube.dev/skyrimse/script/referencealias/function/forcerefto/`.

- Obliga a la alias a usar esa referencia concreta, sin pasar por el `Specific Reference` fijado
  en el editor. Útil si en algún momento se quiere rellenar dinámicamente una de las Aliases de
  material (p. ej. desde el propio SKSE plugin de este proyecto o desde un script de setup) en vez
  de fijarlas todas a mano en el editor.
- **Trampas documentadas**: no sirve para vaciar una alias (pasarle `None` no limpia — usar
  `Clear()`); si la alias pertenece a un actor con package data de escena, llamarlo repetidamente
  mientras una Scene está en marcha puede hacer que el actor pierda sus packages de escena de
  forma permanente; no es una llamada que espere ("yield") a que el cambio se refleje, así que no
  asumir que la alias ya tiene la nueva referencia inmediatamente después de llamarla.

### `GetItemCount(Form akItem)` — `ObjectReference`/`Actor`

Fuente: `https://papyrus.bellcube.dev/skyrimse/script/objectreference/function/getitemcount/`
(cita `https://ck.uesp.net/wiki/GetItemCount_-_ObjectReference`).

- Confirma como función real (ya se citaba en `quest-mechanics-ck-wiki.md` como "patrón de
  comunidad" para detectar que el jugador recogió un material) — esta página sí es documentación
  oficial de la función en sí, aunque su uso concreto para avanzar stages de quest sigue siendo
  patrón de comunidad, no un tutorial oficial de Bethesda.
- `akItem` acepta referencia concreta, base object, lista de formularios (leveled list) o
  **keyword** (cuenta todos los items con esa keyword).
- **Trampas documentadas**: si `akItem` es una keyword y la referencia es un actor, la función
  falla (mitigación citada: volcar el inventario a un contenedor aparte); no funciona bien con
  leveled lists definidas en el editor; llamada desde fuera de la celda de la referencia, devuelve
  `1` en el caso de que el form buscado esté en una leveled list (comportamiento no intuitivo a
  tener en cuenta si el material se comprueba con el jugador lejos de la celda).

## Cómo usar este sitio para lo que quede pendiente de esta quest

Cuando haga falta verificar la firma exacta de cualquier otra función/evento de Papyrus
(`Actor.AddItem`, `Game.GetPlayer`, funciones de `ActiveMagicEffect` para un efecto de la forja,
etc.), el flujo confirmado que funciona en esta sesión es:

1. Adivinar/confirmar el nombre del script en minúsculas (p. ej. `actor`, `game`) y de la
   función/evento en minúsculas sin guiones bajos.
2. `WebFetch` directo a `https://papyrus.bellcube.dev/skyrimse/script/<script>/function/<nombre>/`
   (o `/event/<nombre>/`).
3. Si da 404, el nombre no está indexado tal cual — recurrir entonces al flujo ya documentado en
   `quest-mechanics-ck-wiki.md` (`WebSearch` contra `ck.uesp.net`) o descargar la página de
   `en.uesp.net`/`ck.uesp.net` a mano si aplica.
4. Si hace falta explorar qué miembros tiene un script sin saber el nombre exacto, la página base
   `.../script/<script>/` lista todas sus funciones/eventos/propiedades.
