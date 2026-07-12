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

## Workflow

- Antes de modificar archivos, da un resumen muy breve (1-2 líneas) de qué vas a hacer y por qué.
- Mantén un archivo `CHANGELOG.md` en la raíz del proyecto, en español. Cada vez que implementes una función o cambio relevante (nueva funcionalidad, módulo nuevo, fix importante), añade una entrada breve describiéndolo, con fecha. Usa tu criterio para distinguir cambios relevantes de ajustes menores — no anotes renombrados de variables, arreglos triviales de formato u otros cambios cosméticos. Cada entrada de fecha se subdivide en versiones `0.Y.Z` (lista de cambios, no párrafos): empieza en `0.0.1`; `Y` sube en 1 cada vez que se implementa una mecánica nueva (y `Z` vuelve a 1 en ese momento); `Z` sube en 1 con cada cambio sobre una mecánica ya existente.
- Antes de diseñar o implementar cualquier módulo que afecte a la mecánica del arma (INPUT, WEAPON, THROW, RETURN, PHYSICS, ANIMATION, COMBAT), consulta `Mecanica del arma.txt` y verifica que el diseño no contradice ninguno de sus puntos numerados. Si una decisión no está cubierta por el documento (p. ej. un placeholder temporal a falta de otro módulo), dilo explícitamente en vez de asumir un comportamiento no especificado.

## Arquitectura de física de proyectiles

Decisión tomada al implementar `THROW`/`RETURN`, no recogida en `Mecanica del arma.txt` (que describe intención, no implementación):

- **Ida (lanzamiento)**: usa el sistema nativo de `RE::Projectile` de Skyrim (Havok), afectado por gravedad. Da gratis el arco parabólico (punto 3), la detección de impacto y el clavado en superficie/enemigo (punto 6, mismo mecanismo que usan las flechas al clavarse, incluido ir "pegado" a un NPC que se mueve).
- **Vuelta (regreso)**: NO se reutiliza la física nativa. Los requisitos (curva no balística hacia el jugador, velocidad recalculada según distancia/tiempo para cumplir el límite de 2s del punto 8, desviarse hacia un enemigo dentro de un ángulo máximo en el punto 10, enderezarse antes de llegar en el punto 11) no se pueden conseguir ajustando parámetros de gravedad/velocidad de un cuerpo simulado por Havok — forzar la posición de un objeto simulado activamente por el motor de físicas produce tirones/clipping. En su lugar: al empezar el regreso se destruye el `Projectile` físico y se controla manualmente la posición/rotación cada tick (sin simulación de Havok), incluyendo detección de golpes a enemigos por proximidad propia (punto 9), ya que se pierde la colisión automática del motor.
- El punto de transición entre ambos sistemas coincide con la transición de estado ya existente `kThrown`/`kStuck` → `kReturning` en `WeaponState`, así que no añade complejidad extra a la máquina de estados.
- Coste de rendimiento: el control manual del regreso no es más caro que la física nativa — al contrario, se elimina la simulación de Havok para ese objeto y se sustituye por aritmética simple de posición por fotograma.

### Trampas conocidas (THROW)

- Lanzar con `RE::Projectile::LaunchArrow` + un `Ammo` propio que apunte al `Projectile`, no con `LaunchData`/`Projectile::Launch` a mano: si no, no registra ningún impacto.
- Tipo de proyectil `Arrow`, no `Missile`/`Lobber`(`Grenade`): Lobber no responde a dirección+velocidad (queda flotando); Missile se destruye al chocar en vez de quedar clavado.
- El `Projectile` necesita `Collision Layer` asignado (p. ej. `L_PROJECTILE`) y "Can be Picked Up" desmarcado (si no, se recoge como munición falsa y deja un modelo fantasma).
- Dirección de cámara: vector `GetVectorY()` de `camera->cameraRoot->world.rotate` + `atan2`/`asin`, no `NiMatrix3::ToEulerAnglesXYZ` (orden de ejes distinto al que compone Skyrim; da ángulos mal en cuanto hay inclinación vertical, aunque con solo giro horizontal parezca coincidir).
- "Clavada" = `ImpactResult::kImpale`/`kStick` (vía `skyrim_cast<RE::MissileProjectile*>`), no cualquier valor distinto de `kNone` (`kBounce` sigue en vuelo). Contra actores ese campo no cambia de forma fiable: hace falta además `RE::TESHitEvent` filtrado por `a_event->source` (no `a_event->projectile`, que llega a 0 con `LaunchArrow` + un arma como `a_weap`).
- Nunca reprogramar un sondeo periódico llamando a `SKSE::GetTaskInterface()->AddTask` desde dentro de la propia tarea que se ejecuta: congela el juego por completo. Usar un hilo aparte que duerma un intervalo real y solo entonces llame a `AddTask` (aplica también a `RETURN` si usa el mismo patrón de sondeo).

### Trampas conocidas (WEAPON / EVENTS)

- `RE::ActorEquipManager::EquipObject`/`UnequipObject` quedan en cola por defecto. Incluso forzando aplicación inmediata (`a_queueEquip=false, a_forceEquip=true, a_applyNow=true`), llamarlo síncronamente dentro de ciertos manejadores de eventos (p. ej. al cerrarse una pantalla de carga) falla en silencio — suena el efecto pero no llega a equipar. Hay que diferirlo un tick con `SKSE::GetTaskInterface()->AddTask`.
- Para detectar que el jugador cambió de celda (puerta, viaje rápido...): `TESCellAttachDetachEvent` nunca se dispara para la referencia del jugador (igual que `OnCellAttach`/`OnCellDetach` en Papyrus); `TESCellFullyLoadedEvent` solo salta si el motor carga datos nuevos, así que falla al volver a una celda ya visitada/en caché. La señal fiable es el cierre de `"Loading Menu"` vía `RE::MenuOpenCloseEvent`/`RE::UI`.

## Source layout

`src/` is organized into numbered topic folders (`1.- CORE`, `2.- IMPUT` [sic — historical typo for INPUT, kept for consistency with existing folder], `3.- WEAPON`, `4.- THROW`, `5.- RETURN`, `6.- PHYSICS`, `7.- COMBAT`, `8.- ANIMATION`, `9.- MATH`, `10.- EVENTS`, `11.- SKYRIM`). The numbering is just a stable sort order for browsing — it is **not** a build/dependency sequence, so don't assume module N depends only on modules < N.
