Extracto cacheado (2026-08-29) de datos reales de materiales/objetos de Skyrim, para usar como
inspiración o referencia directa en una quest de "recolectar materiales en distintas partes del
mundo para fabricar un arma". Fuente: **en.uesp.net** (UESP, wiki de referencia de datos de juego
de The Elder Scrolls — no confundir con `ck.uesp.net`, el wiki de la Creation Kit; son dominios
distintos con contenido distinto). A diferencia de `ck.uesp.net`, `en.uesp.net` **no** está detrás
de un challenge de Cloudflare — se pudo descargar el HTML completo con `curl` normal y corriente,
así que esto es contenido verbatim de la página real, no un snippet de búsqueda.

Todo lo de aquí es **dato de juego verificado** (existe en vanilla/DLC oficial tal cual), no una
propuesta de diseño — la sección de propuestas de diseño (qué combinar, qué tema darle) está en
`SKILL.md`, claramente separada y marcada como no verificada donde corresponda.

## Ebony (mena de ébano / lingote de ébano)

Fuente: `https://en.uesp.net/wiki/Skyrim:Ebony_Ore`.

Sustancia negra, dura, tipo vidrio, usada para armas/armaduras de alto nivel (Ebony Smithing
requiere 80 de Smithing). 3 menas de ébano se obtienen minando una veta; 2 menas se funden en 1
lingote.

**Ubicaciones con veta garantizada** (tabla completa de la página; solo las de mayor cantidad):

| Región | Lugar | Vetas | Notas |
|---|---|---|---|
| Eastmarch | Gloombound Mine | 16 | Dentro del stronghold Orc de Narzulbur — la mina de ébano "principal" de Skyrim continental |
| Solstheim (DLC Dragonborn) | Raven Rock Mine | 9 | Dentro de la ciudad de Raven Rock; requiere haber empezado "The Final Descent" |
| Winterhold | Blackreach | 6 | Cerca de las cascadas, al sur |
| The Rift | Redbelly Mine | 3 (+3 lingotes sueltos) | Fuera de Shor's Stone; las menas están fuera de la mina, junto a la fundición |
| Whiterun Hold | Throat of the World | 2 | En la cima de la montaña |
| Hjaalmarch | Labyrinthian | 1 (+3 lingotes) | Mena en Labyrinthian Chasm; lingotes en Labyrinthian, Tribune |

También aparece como botín aleatorio en Falmer (desde nivel 13) y en restos de ash spawn/gargoyle,
y se vende en tiendas de herrero/general a partir de cierto nivel del jugador.

## Stalhrim

Fuente: `https://en.uesp.net/wiki/Skyrim:Stalhrim`.

Hielo nórdico antiguo encantado, usado como material de crafteo. Solo se puede trabajar tras
obtener el perk **Ebony Smithing** y completar la quest **"A New Source of Stalhrim"**. Solo se
puede minar con un **pico nórdico antiguo** ("ancient Nordic pickaxe") — un pico normal no sirve.
Cada depósito da 3 muestras y tarda el doble que una veta normal (6 golpes en vez de 3). No hace
falta fundirlo, se usa tal cual se extrae.

**Exclusivo de Solstheim (DLC Dragonborn) — no hay ninguna ubicación en Skyrim continental.** 19
depósitos en total repartidos por la isla (Stalhrim Source es el mayor, con 10). También lo vende
Baldor Iron-Shaper.

Las armas de Stalhrim hacen el mismo daño que las de ébano pero pesan menos; los encantamientos de
resistencia a hielo/daño de hielo y de daño de caos son un 25% más fuertes sobre objetos de
Stalhrim que sobre cualquier otro material — dato relevante si el tema del arma final tiene
componente de hielo/tormenta.

## Daedra Heart

Fuente: `https://en.uesp.net/wiki/Skyrim:Daedra_Heart`.

Ingrediente de alquimia y de crafteo, clasificado como **"Rare"** por disponibilidad en mercader.
Se obtiene matando **Dremora**. Es también un ingrediente clave para armadura/armas Daédricas,
tanto en forjas normales como en el **Atronach Forge**. La rareza combinada con el uso doble
(alquimia + crafteo daédrico) lo hace valioso — sus fuentes sí respawnean con el tiempo, pero
reunir varias unidades de golpe puede ser lento.

A diferencia de Ebony/Stalhrim, **no está atado a un lugar fijo del mapa** — está atado a un tipo
de enemigo (Dremora), así que como "material de una parte concreta del mundo" encajaría peor salvo
que se diseñe un encuentro/localización específica con Dremora para la quest.

## Aetherium Shard — precedente de diseño más que material suelto

Fuente: `https://en.uesp.net/wiki/Skyrim:Lost_to_the_Ages`.

No es un ingrediente cualquiera: es el objeto central de la quest oficial **"Lost to the Ages"**
(Dawnguard, ID de quest `DLC1LD`, nivel sugerido 16) — **el precedente real de Bethesda más
parecido a lo que se pide en esta conversación**: recolectar varios materiales de crafteo
repartidos por distintas ubicaciones del mundo para fabricar un objeto legendario en una forja
especial.

Resumen verificado de la estructura de la quest (walkthrough, no el árbol de aliases interno):

- El jugador localiza **cuatro Aetherium Shards**, repartidos entre varias ruinas dwemer:
  **Arkngthamz**, **Deep Folk Crossing**, **Raldbthar**, **Dwarven Storeroom** y **Ruins of
  Bthalft** (la página lista estas cinco ubicaciones asociadas a la quest; no todas contienen un
  shard cada una — el propio artículo remite a sus "notes" para el reparto exacto, no asumido
  aquí).
  - Guía la misión el fantasma de una investigadora, **Katria** — un NPC que acompaña/orienta al
    jugador a través de las ruinas.
- Con los shards reunidos, el jugador llega a la **Aetherium Forge**, dentro de **Ruins of
  Bthalft**, y fabrica el objeto legendario.
- Recompensa final: el jugador elige entre tres objetos únicos (**Aetherial Crown**, **Aetherial
  Shield** o **Aetherial Staff**) — un patrón de "elige tu recompensa" al terminar de fabricar,
  también relevante como idea de diseño.

**Cómo verificar la estructura exacta de aliases/stages que usó Bethesda para esto** (recomendado
antes de copiar el patrón a ciegas): esta página de UESP es un walkthrough narrativo para
jugadores, no documentación del editor — no dice qué Reference Aliases o qué números de stage usó
la quest real. Si hace falta ese nivel de detalle, la única fuente fiable es abrir el registro
`DLC1LD` en la propia Creation Kit (con Dawnguard.esm cargado) o en xEdit/SSEEdit, y leer su pestaña
de Aliases/Stages directamente — no asumir la estructura interna a partir del walkthrough.

## Void Salts — ingrediente de alquimia, posible enlace temático (tormenta/rayo)

Fuente: `https://en.uesp.net/wiki/Skyrim:Void_Salts`.

Ingrediente de alquimia, **"Rare"** en mercader, que sueltan los **Storm Atronachs** al morir (su
primer efecto de alquimia es "Weakness to Shock"). No tiene ubicación fija en el mapa — depende de
matar a ese tipo de criatura, igual que Daedra Heart depende de matar Dremora.

**No verificado como ingrediente de crafteo de armas/armadura** (a diferencia de Ebony/Stalhrim/
Daedra Heart, que sí tienen uso de Smithing documentado arriba) — en vanilla es exclusivamente un
ingrediente de Alquimia. Usarlo como requisito de un COBJ de un arma sería una receta nueva
diseñada por el usuario, no la reutilización de un patrón vanilla ya existente — dejarlo explícito
si se propone.
