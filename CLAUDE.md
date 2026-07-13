# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

SKSE plugin (C++23) for Skyrim SE/AE adding a throwable/returning weapon mechanic (Leviathan-Axe style). Built on CommonLibSSE-NG. Currently a skeleton — every file under `src/` is a doc-comment stub with no implementation yet, so there is no existing code style precedent to preserve; use idiomatic modern C++ and CommonLibSSE-NG conventions.

Design intent (throw/return physics, aiming, sticking, animation timing) is documented in `Mecanica del arma.txt` (Spanish) — read it before implementing the throw/return/physics modules.

## Build

Build system is **xmake**, not CMake/MSBuild directly (`xmake.lua` at root). `lib/commonlibsse-ng` is a git submodule (CommonLibVR, branch `ng`) — if it's missing/empty, run `git submodule update --init --recursive` first.

- Quick compile: `xmake` / `xmake build`
- Generate a Visual Studio project for debugging: `xmake project -k vsxmake` (output goes to a gitignored `vs*/` dir)
- No CI is configured; there is no automated test suite (SKSE plugins aren't unit-testable in isolation). Verify changes by building, then deploying the DLL into a Skyrim SE/AE install via a mod manager (MO2) and testing in-game.

## Conventions

- Code comments and commit messages: **Spanish**, matching the existing codebase and design notes.
- Logging: use the `logs::` alias (`namespace logs = SKSE::log;`, set up in `pch.h`), not raw `SKSE::log` calls.
- Formatting: `.clang-format` at the root matches `lib/commonlibsse-ng`'s style (4-space indent, CRLF, no column limit). Run `clang-format -i` on touched files, or format-on-save in your editor.
- License is GPL-3.0 with a linking exception (see `EXCEPTIONS`) — needed because it links against non-GPL modding libraries (SKSE/CommonLibSSE-NG). Keep that exception intact if licensing files are touched.

## Errores comunes a vigilar (generación por IA)

Este proyecto se desarrolla con asistencia de Claude Code. Los siguientes son patrones de error frecuentes en código nativo generado por IA para este tipo de plugin — revisar activamente cada vez, no asumir que no van a aparecer:

- **APIs inventadas**: no dar por buena la firma de una función/clase de CommonLibSSE-NG de memoria. Verificar contra los headers reales en `lib/commonlibsse-ng` (grep/búsqueda directa) antes de usarla, especialmente si existe en varias forks (Sample, Fudge, NG) con diferencias.
- **Offsets/direcciones hardcodeadas**: nunca usar direcciones de memoria fijas (`0x140XXXXXX`). Siempre `REL::RelocationID` / `REL::Relocation<>` con Address Library, para no romper compatibilidad SE/AE/VR.
- **Punteros nulos sin comprobar**: cualquier resultado de búsqueda de formulario/objeto (`TESForm::LookupByID`, casts con `skyrim_cast`, etc.) debe comprobarse antes de usarse. Un `nullptr` sin verificar aquí no lanza excepción manejable, crashea el juego entero.
- **Hilos y el motor**: no asumir que el motor de Skyrim es thread-safe. Cualquier llamada que toque el juego (formularios, actores, UI) debe ejecutarse en el hilo principal vía `SKSE::GetTaskInterface()->AddTask` salvo que se sepa explícitamente que es segura desde otro hilo (ver también la trampa de `AddTask` reentrante en "Arquitectura de física de proyectiles" más abajo).
- **Gestión de memoria**: preferir RAII y los wrappers de CommonLibSSE-NG sobre `new`/`delete` manual. Si se instala un hook/trampolín, comprobar que restaura el estado original correctamente y que no colisiona con offsets que puedan estar parcheados por otro mod/librería.
- **Excepciones cruzando el motor**: el código nativo de Skyrim no espera excepciones de C++ propagándose desde callbacks del motor (eventos, hooks). Si una función puede lanzar (parseo de datos externos, por ejemplo), capturarla localmente en vez de dejar que suba.
- **Serialización del cosave** (si se implementa guardado de datos vía `SKSE::SerializationInterface`): versionar siempre los registros (`WriteRecord` con número de versión) para poder migrar en el futuro, comprobar el valor de retorno de `Load`/`Write`, y manejar explícitamente el caso de partidas guardadas antiguas sin esos datos.
- Si no se puede verificar que una función/clase/comportamiento existe tal cual en el código fuente del proyecto o de CommonLibSSE-NG, decirlo explícitamente en vez de asumir que existe — mismo criterio que ya se pide en Workflow para el documento de diseño.

## Workflow

- Antes de modificar archivos, da un resumen muy breve (1-2 líneas) de qué vas a hacer y por qué.
- Mantén un archivo `CHANGELOG.md` en la raíz del proyecto, en español. Cada vez que implementes una función o cambio relevante (nueva funcionalidad, módulo nuevo, fix importante), añade una entrada breve describiéndolo, con fecha. Usa tu criterio para distinguir cambios relevantes de ajustes menores — no anotes renombrados de variables, arreglos triviales de formato u otros cambios cosméticos. Cada entrada de fecha se subdivide en versiones `0.Y.Z` (lista de cambios, no párrafos): empieza en `0.0.1`; `Y` sube en 1 cada vez que se implementa una mecánica nueva (y `Z` vuelve a 1 en ese momento); `Z` sube en 1 con cada cambio sobre una mecánica ya existente.
- Antes de diseñar o implementar cualquier módulo que afecte a la mecánica del arma (INPUT, WEAPON, THROW, RETURN, PHYSICS, ANIMATION, COMBAT), consulta `Mecanica del arma.txt` y verifica que el diseño no contradice ninguno de sus puntos numerados. Si una decisión no está cubierta por el documento (p. ej. un placeholder temporal a falta de otro módulo), dilo explícitamente en vez de asumir un comportamiento no especificado.

## Arquitectura de física de proyectiles

- **Ida**: `RE::Projectile` nativo (Havok, vía `Projectile::LaunchArrow`) — da gratis el arco parabólico y el clavado (puntos 3 y 6).
- **Vuelta**: no reutiliza el `Projectile` de la ida bajo ningún concepto, en ninguno de los tres puntos de partida (en vuelo, clavado en superficie, clavado en un actor). Probado en el juego (varias rondas, ver `CHANGELOG.md` en torno a la v0.3.x): un `Projectile` todavía activo en Havok en el momento de tomar control (en vuelo, o recién relanzado vía `LaunchArrow`) tiene su propia lógica interna de vuelo balístico que compite con el control manual tick a tick — ni `SetMotionType(kKeyframed)` (con `a_force=true` incluso), ni escribir directamente el `bhkRigidBody`, ni forzar `Update3DPosition`, lo evitan de forma fiable; el arma se queda congelada/parpadeando en vez de volar. Un `Projectile` ya en reposo (clavado en superficie) no tenía este problema, pero no hay forma de garantizar ese estado de partida en los otros dos casos.
  - En su lugar, al pulsar recuperar se captura la posición actual, se destruye/desengancha lo que hubiera (`Projectile::Kill()`, o `Throw::DetachEmbeddedWeapon` para el nodo clavado en un actor), y se lanza una **réplica visual nueva sin física de proyectil** con `Throw::SpawnWeaponReplicaAt` (`TESObjectREFR::PlaceObjectAtMe` con el propio `TESObjectWEAP` — una referencia normal, no un `Projectile`, así que no tiene la lógica de vuelo balístico que competía). Esa réplica sí se controla con `SetMotionType(kKeyframed)` + `SetPosition`/`Update3DPosition` cada tick sin problema, porque no hay nada más escribiendo su posición.
  - Como con cualquier referencia recién creada, su `Get3D()` puede tardar unos frames en cargar en segundo plano; hay que esperar a que deje de ser nulo antes de empezar a moverla (ver `Return::WaitFor3DThenStart`).

### Dos cosas que RETURN necesita

- **No hay ningún hook de "por fotograma" en este proyecto.** El patrón usado en `THROW` y `RETURN` para el control manual por tick: un `std::thread` que duerme un intervalo real y solo entonces reencola en el hilo principal vía `SKSE::GetTaskInterface()->AddTask`. Nunca llamar `AddTask` desde dentro de la propia tarea que se ejecuta (congela el juego).
- **El `Projectile` no sobrevive a un impacto contra un actor** (el motor lo destruye al procesar el golpe). Lo que queda clavado es un nodo 3D (`"Scene Root"`, sin renombrar) enganchado al hueso del golpe con ~1s de retraso — se localiza por nombre en `actor->Get3D()` y se quita con `NiNode::DetachChild()` (ver `Throw::DetachEmbeddedWeapon`, reutilizable).

### Trampas conocidas (WEAPON / EVENTS)

- `RE::ActorEquipManager::EquipObject`/`UnequipObject` quedan en cola por defecto. Incluso forzando aplicación inmediata (`a_queueEquip=false, a_forceEquip=true, a_applyNow=true`), llamarlo síncronamente dentro de ciertos manejadores de eventos (p. ej. al cerrarse una pantalla de carga) falla en silencio — suena el efecto pero no llega a equipar. Hay que diferirlo un tick con `SKSE::GetTaskInterface()->AddTask`.
- Para detectar que el jugador cambió de celda (puerta, viaje rápido...): `TESCellAttachDetachEvent` nunca se dispara para la referencia del jugador (igual que `OnCellAttach`/`OnCellDetach` en Papyrus); `TESCellFullyLoadedEvent` solo salta si el motor carga datos nuevos, así que falla al volver a una celda ya visitada/en caché. La señal fiable es el cierre de `"Loading Menu"` vía `RE::MenuOpenCloseEvent`/`RE::UI`.

## Source layout

`src/` is organized into numbered topic folders (`1.- CORE`, `2.- IMPUT` [sic — historical typo for INPUT, kept for consistency with existing folder], `3.- WEAPON`, `4.- THROW`, `5.- RETURN`, `6.- PHYSICS`, `7.- COMBAT`, `8.- ANIMATION`, `9.- MATH`, `10.- EVENTS`, `11.- SKYRIM`). The numbering is just a stable sort order for browsing — it is **not** a build/dependency sequence, so don't assume module N depends only on modules < N.
