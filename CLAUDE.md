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

## Source layout

`src/` is organized into numbered topic folders (`1.- CORE`, `2.- IMPUT` [sic — historical typo for INPUT, kept for consistency with existing folder], `3.- WEAPON`, `4.- THROW`, `5.- RETURN`, `6.- PHYSICS`, `7.- COMBAT`, `8.- ANIMATION`, `9.- MATH`, `10.- EVENTS`, `11.- SKYRIM`). The numbering is just a stable sort order for browsing — it is **not** a build/dependency sequence, so don't assume module N depends only on modules < N.
