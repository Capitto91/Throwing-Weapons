---
name: skse-plugin-pitfalls
description: Revisa código de plugins SKSE para Skyrim SE/AE/VR (C++, CommonLibSSE / CommonLibSSE-NG) en busca de malas prácticas conocidas que causan crashes, incompatibilidades entre mods o roturas al actualizar el juego. Úsala siempre que el usuario pida revisar, auditar, hacer code review, o "detectar problemas" en un plugin SKSE, un hook, un event sink, código de serialización/cosave, o cualquier archivo .cpp/.h de un proyecto que use SKSE/CommonLibSSE, incluso si no lo pide explícitamente con la palabra "skill".
---

# Auditoría de malas prácticas en plugins SKSE

Este documento recoge patrones problemáticos, documentados por la comunidad de
desarrollo de SKSE (foros de Nexus, issues/discussions de GitHub de
CommonLibSSE y CommonLibSSE-NG, skyrim.dev, y tutoriales de SkyrimScripting),
que aparecen una y otra vez en plugins nuevos y causan crashes, incompatibilidad
entre mods o roturas cada vez que Bethesda actualiza el juego.

Cuando revises un plugin SKSE, recorre cada sección como checklist. Para cada
hallazgo, explica: (1) qué línea/patrón es problemático, (2) por qué es un
problema real (no solo "es feo"), y (3) cómo se resuelve normalmente en la
comunidad. No te limites a listar sin más — la razón detrás de cada regla
importa para saber cuándo aplica y cuándo no.

## 1. Direcciones de memoria hardcodeadas en vez de Address Library

**Detectar:** literales hexadecimales usados como direcciones de función/objeto
(`REL::ID(0x...)` con un ID que no viene de una base de datos versionada, o
peor, punteros `0x14XXXXXXX` calculados a mano), especialmente si no hay ningún
fallback si el juego se actualiza.

**Por qué importa:** una dirección hardcodeada solo es válida para la versión
exacta de `SkyrimSE.exe` contra la que se compiló. En cuanto Bethesda saca un
parche, el offset cambia y el plugin corrompe memoria o crashea en vez de
fallar de forma controlada. Esta es la razón de ser de Address Library: mapea
IDs estables a direcciones absolutas por versión del ejecutable.

**Qué buscar en su lugar:** uso de `REL::RelocationID` / `REL::ID` con IDs de
Address Library, o directamente CommonLibSSE-NG (que ya lo integra). Si el
plugin usa CommonLibSSE-NG, esto normalmente ya está resuelto — marca como
problema solo si ves acceso directo a memoria sin pasar por `REL::`.

**Señal de alerta adicional:** el plugin no comprueba el resultado de la
consulta a la base de datos de offsets. Debe fallar la carga del plugin (return
`false` desde `SKSEPluginLoad`) si una dirección no se resuelve, en vez de
seguir con un puntero nulo o inválido.

## 2. FormID absolutos hardcodeados (ignorando el load order)

**Detectar:** un FormID de 8 dígitos completo escrito literalmente en el
código (p. ej. `0x0401ABCD`) usado para buscar un form de un plugin que no sea
`Skyrim.esm`.

**Por qué importa:** el byte alto de un FormID depende de la posición del
plugin en el load order (y con ESLs esto se complica más: el índice de carga
"real" no coincide con el índice de plugin que ve el usuario). Ese mismo form
puede tener un FormID distinto en cada partida según qué otros mods estén
instalados. Un FormID absoluto hardcodeado apunta a un form aleatorio (o a
nada) en cuanto cambia el load order.

**Qué buscar en su lugar:** el FormID *relativo* (los 6 dígitos bajos, sin el
byte de plugin) combinado con el nombre del archivo de origen —
`RE::TESDataHandler::GetSingleton()->LookupForm(id, "Plugin.esp")` en C++, el
equivalente a `Game.GetFormFromFile` en Papyrus. Esto es independiente del
load order.

## 3. Llamar funciones del motor / Papyrus fuera del hilo principal

**Detectar:** código dentro de un hook, un callback de hilo secundario, o un
`std::thread` propio que llama directamente a funciones que mutan el estado
del juego (equipar objetos, mover referencias, tocar el VM de Papyrus, tocar
UI) sin pasar por ningún mecanismo de sincronización.

**Por qué importa:** buena parte del motor de Skyrim (incluida la VM de
Papyrus) no es thread-safe. Llamar funciones que alteran el juego desde el
hilo equivocado, o más de una vez por frame de forma no sincronizada, produce
condiciones de carrera y crashes intermitentes muy difíciles de reproducir.

**Qué buscar en su lugar:** `SKSE::GetTaskInterface()->AddTask(...)` para
reencolar el trabajo en el hilo principal del juego antes de tocar nada que
mute estado. Si el plugin registra callbacks nativos para Papyrus, deben
considerar que pueden ejecutarse en un tasklet fuera del hilo principal.

## 4. Registrar event sinks demasiado pronto, o no desregistrarlos

**Detectar:** llamadas a `AddEventSink` (o registro de listeners de mensajería)
dentro de `SKSEPluginLoad` / al cargar la DLL, antes de que el juego haya
cargado los datos. También: sinks que nunca se quitan (`RemoveEventSink`) o
que se registran en cada evento en vez de una sola vez.

**Por qué importa:** en el momento en que se carga la DLL (`SKSEPluginLoad`)
todavía no existen los datos del juego (forms, TESDataHandler, etc.). Acceder
a ellos ahí devuelve punteros nulos o inválidos. El registro de sinks y el
acceso a forms debe esperar al mensaje `kDataLoaded` de la
`MessagingInterface`. Además, si un sink no se desregistra cuando su objeto se
destruye, el dispatcher del juego puede terminar llamando a memoria liberada
(use-after-free).

**Qué buscar en su lugar:** un listener de `SKSE::GetMessagingInterface()`
que reacciona a `kDataLoaded` (o `kPostLoad`/`kPostPostLoad` según el caso) y
es ahí donde se registran los sinks. Ojo también: varias librerías wrapper
(como `SkyrimScripting::Plugin::Initialize()`) solo permiten registrar **un**
listener de mensajería propio — si el plugin usa esa librería y además llama
a `RegisterListener` por su cuenta, uno de los dos se pisa.

## 5. Hooks sin gestionar el espacio del trampolín ni pensar en compatibilidad

**Detectar:** hooks escritos a mano tocando bytes de la función objetivo sin
usar el `TrampolineInterface`/`SKSE::AllocTrampoline`, o sin reservar
suficiente espacio antes de escribir el hook.

**Por qué importa:** varios plugins pueden querer hookear el mismo punto del
juego. El trampolín de SKSE existe para escribir hooks de forma que coexistan
con los de otros plugins de manera predecible. Un hook que sobreescribe bytes
directamente (sin trampolín) puede corromper otro hook ya instalado por otro
mod, o quedarse sin espacio y crashear al cargar.

**Qué buscar en su lugar:** `SKSE::AllocTrampoline(N)` con un tamaño
razonable reservado *antes* de instalar los hooks, y uso de las utilidades de
hooking de CommonLibSSE (`REL::Relocation`, `stl::write_thunk_call`, etc.) en
vez de escritura manual de bytes.

## 6. Cosave/serialización mal implementada

**Detectar:** un plugin que guarda datos propios en la partida (cosave) pero:
usa un ID de plugin que no es único o parece copiado de un tutorial sin
cambiar, no implementa el callback de "revert" (limpiar el estado al cargar
una partida nueva o distinta), o asume que los datos guardados siempre existen
al cargar (no maneja el caso de que el save no tenga esa sección, p. ej. si el
mod se añadió después).

**Por qué importa:** cada plugin necesita un ID único para su bloque de datos
en el cosave; si colisiona con el de otro plugin, ambos pueden corromper los
datos del otro. No limpiar el estado en el callback de revert dejas datos
"fantasma" de una partida anterior. Y no comprobar la ausencia de datos rompe
la compatibilidad con partidas guardadas antes de instalar el mod.

**Qué buscar en su lugar:** registro con `SKSESerializationInterface` /
`SKSE::GetSerializationInterface()`, con callbacks de save, load y **revert**
los tres implementados, un ID de plugin propio y verificado como único, y
manejo explícito del caso "no hay datos guardados todavía".

## 7. Excepciones sin capturar dentro de hooks o callbacks

**Detectar:** código dentro de un hook, un event sink o una función nativa de
Papyrus que puede lanzar una excepción (acceso a un contenedor vacío,
`std::stoi` sobre texto no numérico, un `dynamic_cast`/`DYNAMIC_CAST` fallido
usado sin comprobar nulo, etc.) sin ningún `try/catch` alrededor.

**Por qué importa:** el plugin corre *dentro* del mismo proceso que
`SkyrimSE.exe`. Una excepción no capturada ahí no se queda contenida en el
plugin: tira abajo el proceso completo del juego, con la consiguiente pérdida
de progreso de esa sesión.

**Qué buscar en su lugar:** manejo defensivo en cualquier punto de entrada que
el motor pueda invocar (hooks, sinks, funciones nativas), y comprobación
explícita de punteros nulos en vez de asumir que un cast o una búsqueda de
form siempre tiene éxito.

## 8. Punteros colgantes a forms/referencias

**Detectar:** guardar un `Actor*`, `TESObjectREFR*` u otro puntero crudo en una
variable de larga duración (miembro de clase, caché estático) y usarlo más
tarde sin volver a comprobar su validez.

**Por qué importa:** referencias y actores pueden desaparecer (morir, ser
eliminados, la celda se descarga) en cualquier momento entre que guardas el
puntero y lo vuelves a usar. Usarlo después es un use-after-free clásico:
puede "funcionar" en pruebas y crashear de forma intermitente para los
usuarios.

**Qué buscar en su lugar:** guardar el `FormID` (o un `RE::FormID` +
`RE::TESForm::LookupByID`) en vez del puntero crudo para accesos diferidos, o
usar `RE::NiPointer`/`RE::ObjectRefHandle` donde la API lo soporte, y
revalidar antes de cada uso.

## 9. No usar CommonLibSSE-NG cuando no hay una razón concreta para no hacerlo

**Detectar:** un proyecto nuevo que usa CommonLibSSE "clásico" con submódulo
git, offsets propios, o SKSE crudo sin ninguna capa de abstracción — y sin que
el propio autor mencione una razón de peso (p. ej. mantener compatibilidad con
un fork muy específico).

**Por qué importa:** CommonLibSSE-NG es el fork mantenido activamente que
soporta SE, AE y VR desde una única DLL, con Address Library integrado y RTTI
regenerado. Reinventar esa capa a mano es mucho trabajo extra y una fuente
constante de bugs de compatibilidad entre versiones del juego.

**Nota:** esto es una recomendación, no una prohibición — señala el patrón
pero no lo marques como "error" si el proyecto tiene una razón documentada
para no usarlo.

## 10. Trabajo pesado en cada frame en vez de dirigido por eventos

**Detectar:** lógica costosa (recorrer inventarios grandes, buscar forms,
tocar disco) dentro de un hook que se ejecuta cada frame o cada tick, en vez
de reaccionar a un evento puntual o usar un temporizador con intervalo
razonable.

**Por qué importa:** el hilo principal de Skyrim ya está ajustado en
presupuesto de tiempo por frame; trabajo innecesario ahí genera microstutter
perceptible, y con muchos mods activos el coste se acumula.

**Qué buscar en su lugar:** engancharse al evento específico que de verdad le
interesa al plugin (p. ej. `TESEquipEvent`, `kDataLoaded`, cambio de celda) en
vez de sondear (`polling`) en cada frame.

---

## Formato de salida sugerido al revisar un plugin

Para cada hallazgo:

```
### [Categoría, p. ej. "FormID hardcodeado"]
**Dónde:** archivo:línea o fragmento de código
**Problema:** explicación breve, en tus propias palabras, de por qué esto es un riesgo
**Sugerencia:** qué patrón usar en su lugar
**Severidad:** alta (crash / corrupción probable) · media (incompatibilidad entre mods) · baja (rendimiento / estilo)
```

Si no encuentras problemas de una categoría, no la menciones — no rellenes el
informe con secciones vacías. Si el código ya sigue el patrón recomendado en
alguna categoría, no hace falta señalarlo como acierto salvo que el usuario lo
pida explícitamente.
