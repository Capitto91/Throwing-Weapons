Extracto verificado (2026-09-20) de **`https://wiki.fireundubh.com/papyrus-anti-patterns`**
("Papyrus Anti-Patterns", *Let's Play with Fire Wiki*), aportado por el usuario como base para un
código de buenas prácticas al escribir scripts Papyrus de la quest.

## Qué es y cuánto fiarse

- Autor: `fireundubh` (el propio wiki lo declara). Índice actualizado 2020-10-05; las subpáginas,
  entre 2020-10-05 y 2020-11-29. Está "modelado según *The Little Book of Python Anti-Patterns*"
  (cita del propio índice). Es una guía de **comunidad**, no documentación oficial de Bethesda —
  mismo nivel de confianza que `wiki.beyondskyrim.org` (ver `SKILL.md`, "Convenciones de scripting
  Papyrus").
- **La página no dice para qué juego es cada ejemplo, y mezcla Fallout 4 con Skyrim.** Varios
  ejemplos usan sintaxis que el compilador de Skyrim SE rechaza (tabla siguiente). No copiar código
  de esa wiki tal cual: pasar siempre por la columna "Skyrim SE" de más abajo.
- Es un índice de 13 subpáginas + 2 enlaces rotos (los dos de "Readability", indentación con
  espacios/tabs, están marcados `is-invalid-page` en el propio índice: **no existen, no hay
  contenido que citar**).

## Cómo descargarla (no es obvio)

Es un **Wiki.js** (SPA de Vue). Confirmado en esta sesión:

- `WebFetch` sobre el índice devuelve solo el título (el cuerpo no llega al resumen). No es un
  bloqueo — el HTML sí trae el cuerpo.
- `curl -sL -A "Mozilla/5.0" <url>` sí devuelve el cuerpo ya renderizado en el servidor, dentro de
  `<template slot="contents">…</template>`. Hay que bajar **cada subpágina por separado**
  (`https://wiki.fireundubh.com/papyrus-anti-patterns/<slug>`); el índice solo tiene enlaces.
- No hay atajo: `/s/en/<path>` (vista de código fuente) → `403`; `POST /graphql` con
  `pages.singleByPath` → `PageViewForbidden 6013`.

Slugs (los 13 existentes): `not-returning-a-value-on-all-code-paths`,
`not-using-a-break-condition-in-a-loop`, `not-validating-arrays-before-accessing-array-elements`,
`not-validating-objects-or-arguments-before-calling-functions`,
`not-validating-the-length-of-a-dynamic-array`,
`passing-a-zero-denominator-to-a-division-or-modulus-operation`, `not-removing-unused-code`,
`using-a-compound-conditional-statement-instead-of-logic-gates`,
`using-a-single-letter-to-name-your-variables`,
`using-a-variable-to-store-a-property-value-used-once`,
`using-multiple-code-paths-in-boolean-functions`, `using-nested-loops`,
`passing-the-player-reference-to-the-activate-function`.

## Verificación contra el compilador real de Skyrim SE (lo que la fuente da por supuesto)

Compilador: `D:\Steam\steamapps\common\Skyrim Special Edition\Papyrus Compiler\PapyrusCompiler.exe`,
fuentes vanilla en `...\Data\Scripts\Source`, flags `TESV_Papyrus_Flags.flg`. Cada fila es un script
mínimo compilado de verdad (2026-09-20), no una suposición:

| Código de la fuente | Resultado en Skyrim SE (mensaje literal del compilador) |
|---|---|
| `GlobalVariable Property G Auto Const` | **Error**: `Unknown user flag Const` — `Const` es de Fallout 4. En Skyrim: `Auto`, o `AutoReadOnly` para constantes (`Int Property MaxTries = 5 AutoReadOnly` compila limpio) |
| `new ObjectReference[0]` (array "dinámico") | **Error**: `arrays must be between 1 and 128 elements in size` |
| `new Form[129]` | **Error**: el mismo mensaje — tamaño máximo 128 |
| `DynamicArray.Add(kItem, 1)` | **Error**: `Add is not a function or does not exist` |
| Función `Bool` con una ruta sin `Return` | **Compila sin ningún aviso** (`0 error(s), 0 warning(s)`); en el `.pas` la función termina sin instrucción `RETURN` en esa ruta. La advertencia `Assigning None to an non-object variable named "::temp21"` que cita la fuente **no sale** en el compilador de Skyrim — aquí no hay red de seguridad |
| `If R && R.IsDisabled()` | **Compila**, y el bytecode confirma **cortocircuito**: `CAST ::temp0 ::R_var` → `JUMPF ::temp0 label1` salta por encima de `CALLMETHOD IsDisabled` si `R` es `None` |
| `Return !akRef.Is3DLoaded()` | Compila limpio (es el arreglo que propone la fuente) |

Qué **no** se pudo comprobar (requiere ejecutar el juego, no el compilador): los mensajes de error en
tiempo de ejecución que cita la fuente (`Cannot call Activate() on a None object, aborting function
call`, `Cannot access an element of a None array`, `Cannot divide by zero`) y qué devuelve exactamente
la VM cuando una función tipada termina sin `Return`. Se citan como "según la fuente".

**Cómo repetir una prueba así** (para cualquier duda de sintaxis, en vez de fiarse de memoria):
escribir un `.psc` mínimo en el scratchpad y compilarlo. Trampas confirmadas:
- Desde PowerShell, `& PapyrusCompiler.exe …` envuelve stderr en `NativeCommandError` y se pierden
  los mensajes: usar `cmd /c "\"<compilador>\" X.psc \"-i=<carpeta del .psc>;<Scripts\Source>\"
  \"-o=<salida>\" -f=TESV_Papyrus_Flags.flg 2>&1"`.
- `-i` (import) debe incluir la carpeta del propio `.psc`, además de `Scripts\Source`.
- `-keepasm` deja el `.pas` (bytecode legible) junto al `.pex`; sin él se borra. Es la forma de
  comprobar qué compila realmente una expresión (así se confirmó el cortocircuito).

## Catálogo de los 13 anti-patrones, con veredicto para Skyrim SE

### Correctness

1. **No devolver valor en todas las rutas** — *Aplica, con más razón en Skyrim* (el compilador no
   avisa, ver tabla). Regla: toda ruta de una función tipada acaba en `Return`. Mejor aún, cuando
   se pueda, devolver la expresión directa (`Return !akRef.Is3DLoaded()`).
2. **Bucle sin condición de salida anticipada** — *Aplica.* Papyrus no tiene `break`; la fuente usa
   un flag `Bool bBreak` en la condición del `While` (`While (i < n) && !bBreak`). Cita de
   desarrollador que trae la fuente, atribuida a *SmkViper* (no verificada por separado): un
   `While` mantiene vivo el hilo con su pila; *"If you end up with >100 threads running at once for
   over a few seconds, Papyrus assumes something is wrong and starts spitting out stack dumps"*, y
   un `While` que no duerme (`Wait` u otra llamada *latent*) puede provocar *script lag*. (El
   ejemplo de la fuente llama a `GameStateIsValid()`, función inventada como marcador: no existe.)
3. **No validar un array antes de acceder a sus elementos** — *Aplica.* `If Arr != None` antes de
   indexar/leer `.Length`. Según la fuente, indexar un array `None` loguea `Cannot access an element
   of a None array`. Pertinente sobre todo para properties de array sin rellenar en la CK.
4. **No validar objetos/argumentos antes de llamar a una función** — *Aplica.* `If kItem && DummyRef`
   antes de `kItem.Activate(DummyRef)`. Según la fuente, si es `None` Papyrus aborta la llamada y lo
   loguea (no es un crash, pero el efecto pretendido se pierde en silencio). Como `&&` hace
   cortocircuito (verificado), `If R && R.IsDisabled()` es seguro.
5. **No validar la longitud de un array dinámico** — ***No aplica a Skyrim.*** Los arrays dinámicos
   (`new T[0]`, `.Add`) son de Fallout 4; en Skyrim no compilan (ver tabla). Lo que sí sobrevive:
   los arrays de Skyrim tienen **tamaño fijo 1–128** decidido al crearlos, así que hay que respetar
   `i < Arr.Length` y no diseñar nada que necesite más de 128 elementos en un array.
6. **Denominador cero en división/módulo** — *Aplica* (regla general; el mensaje concreto no se pudo
   reproducir sin el juego). Comprobar `!= 0.0` y sustituir por un valor no nulo antes de dividir.
   Ojo: el ejemplo de la fuente cierra la `Function` con `EndIf` (errata suya, no copiar).

### Maintainability

7. **No eliminar código sin usar** — *Aplica.* Borrar funciones/properties obsoletas; guardar lo
   viejo fuera del `.psc`. Efecto verificable según la fuente: quitar properties sin usar evita los
   avisos de "unused property" en el log.
8. **Condicional compuesto en vez de "puertas lógicas"** (`If a || b || c` → un `If` por condición)
   y 9. **Varias rutas de retorno en una función booleana** (`If cond Return True EndIf / Return
   False` → `Return cond`) — *Se contradicen entre sí* (una separa condiciones, la otra las
   condensa en una sola expresión), y la fuente no lo reconoce. **Resolución adoptada aquí, decisión
   propia y no de la fuente**: en una función que solo devuelve un `Bool`, devolver la expresión
   directamente (nº 9); reservar la separación en varios `If` (nº 8) para cuando cada condición
   dispare una acción distinta o la expresión sea larga y difícil de leer. Si el usuario prefiere
   otro criterio, cambiarlo aquí.
10. **Nombres de una sola letra** — *Aplica.* Nombres descriptivos. Los ejemplos de la fuente usan
    prefijos de tipo: `ak`/`as`/`ai`/`af`/`ab` en **parámetros** (`akRef`, `afSkillPenalty`; misma
    convención que los eventos vanilla, `akActionRef`) y `k`/`f`/`b` en locales (`kItem`, `fBestSkill`,
    `bBreak`). **Ojo**: solo la de los parámetros es convención firme de Bethesda; para locales y
    variables de script el propio fireundubh escribió (AFK Mods, 2016) que no hay uniformidad — ver
    `papyrus-community-practices.md`, sección 10.
11. **Variable para guardar el valor de una property usada una sola vez** — *Aplica, menor.*
    Referenciar la property directamente si solo se usa en una rama. Errata de la fuente: el
    ejemplo cierra la `Function` con `EndEvent`.

### Performance

12. **Bucles anidados** — *Aplica.* Coste multiplicativo (la fuente: 20×20 = hasta 420 iteraciones,
    y se dispara con 3-5 niveles). Preguntarse si hace falta recorrer todo; si sí, buscar
    optimización. La alternativa que ofrece la fuente: en vez de recorrer un `FormList` dentro de
    otro bucle, usar `FormList.HasForm(kItem)` sobre properties `FormList`. (Ojo: en su ejemplo
    usa `Property … Auto Const`, que en Skyrim es `Auto`.)
13. **Pasar la referencia del jugador a `Activate`** — ***No aplica a Skyrim.*** La propia página
    índice la etiqueta "Fallout 4" y habla de un problema de motor de ese juego (cuelgues al
    hacerlo de forma rápida y frecuente; workaround: NPC ficticio en una celda aparte). No hay
    verificación de que exista en Skyrim: no aplicar el workaround, pero tampoco afirmar que
    `Activate(PlayerREF)` sea seguro en cualquier caso.

## Erratas de la propia fuente (no copiar tal cual)

- `Function CanUnlock() … EndIf` (nº 6) y `Function DoSomething(…) … EndEvent` (nº 11): cierres
  equivocados; el compilador los rechazaría.
- `Auto Const`, `new T[0]` y `.Add(...)`: sintaxis de Fallout 4 (tabla de verificación).
- `AddInventoryEventFilter(None)` (ejemplo del nº 13): la CK Wiki dice que *"Skyrim does not support
  passing None to this function; that was added in Fallout 4"* (idéntico efecto con una `FormList`
  vacía). ✅ Compila sin aviso en Skyrim (probado), así que el compilador no lo delata. Ver
  `papyrus-community-practices.md`, sección 3.
- Indentación con tabs y espacios mezclados en varios ejemplos (irónico dado que sus páginas de
  "Readability" no existen).
