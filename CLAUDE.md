# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

SKSE plugin (C++23) for Skyrim SE/AE adding a throwable/returning weapon mechanic (Leviathan-Axe style). Built on CommonLibSSE-NG. Currently a skeleton — every file under `src/` is a doc-comment stub with no implementation yet, so there is no existing code style precedent to preserve; use idiomatic modern C++ and CommonLibSSE-NG conventions.

Design intent (throw/return physics, aiming, sticking, animation timing) is documented in `Mecanica del arma.txt` (Spanish) — read it before implementing the throw/return/physics modules.

**Nota sobre el estado del código**: hubo una primera iteración completa (ida con `RE::Projectile` nativo, vuelta con control manual) que llegó a implementar y probar en el juego todos los puntos del documento de diseño hasta el 11. Se descartó por completo (código reseteado a este esqueleto) al decidir abandonar `RE::Projectile` también para la ida — ver "Arquitectura de física de proyectiles" más abajo para la razón y todo lo aprendido/verificado en esa iteración, que sigue siendo válido aunque el código ya no exista. El histórico detallado de esa iteración (qué se probó, qué falló y por qué) está en `CHANGELOG.md`.

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

**Decisión (tras una primera iteración completa que sí usaba `RE::Projectile` nativo para la ida): ni la ida ni la vuelta usan `RE::Projectile`.** Las dos direcciones se controlan a mano, con el mismo patrón, sobre una referencia normal (`TESObjectREFR`) del propio `TESObjectWEAP` — nunca un `Projectile`. Motivos, confirmados en el juego a lo largo de varias iteraciones:

- Un `Projectile` todavía activo en Havok en el momento de tomar control manual (en vuelo, o recién relanzado vía `LaunchArrow`) tiene su propia lógica interna de vuelo balístico que compite con el control manual tick a tick — ni `SetMotionType(kKeyframed)` (con `a_force=true` incluso), ni escribir directamente el `bhkRigidBody`, ni forzar `Update3DPosition`, lo evitan de forma fiable; el arma se queda congelada/parpadeando en vez de volar.
- Tampoco sirve para un giro/tumbado (punto 11): imprimir una velocidad angular con `bhkRigidBody::SetAngularVelocity` a un `Projectile` nativo recién lanzado, una única vez y sin más control manual, **no produce giro visible** (probado en el juego) — el motor aparentemente sigue reorientando el proyectil según su velocidad cada fotograma (comportamiento interno no expuesto), igual que hacen las flechas reales (apuntan siempre hacia donde vuelan, sin tumbarse).
- Perder el `Projectile` nativo también significa perder gratis: arco parabólico (Havok), detección de impacto (`ImpactResult`/`TESHitEvent`) y aplicación de daño con todo el feedback de combate (reacciones de golpe, tambaleo, números de daño, aviso de combate). Ver más abajo qué hay que reconstruir a mano y con qué API verificada.

### El patrón de control manual (usado igual en ida y vuelta)

1. Crear la réplica visual con `TESObjectREFR::PlaceObjectAtMe` sobre el propio `TESObjectWEAP` — una referencia normal, sin la lógica de vuelo balístico de un `Projectile`.
2. Como con cualquier referencia recién creada, su `Get3D()` puede tardar unos frames en cargar en segundo plano; hay que esperar a que deje de ser nulo antes de empezar a moverla (sondeo con el mismo hilo-que-duerme-y-reencola del punto 3).
3. **No hay ningún hook de "por fotograma" en este proyecto.** Para el control manual por tick: un `std::thread` que duerme un intervalo real (~16ms para movimiento fluido) y solo entonces reencola en el hilo principal vía `SKSE::GetTaskInterface()->AddTask`. Nunca llamar `AddTask` desde dentro de la propia tarea que se ejecuta (congela el juego).
4. `node3D->SetMotionType(RE::hkpMotion::MotionType::kKeyframed, true, true, true)` — modo Havok "movido por código" (sin fuerzas/gravedad, conserva colisión). Desactivar la colisión del todo (en vez de kKeyframed) reintrodujo tirones/clipping en pruebas anteriores — mejor mantenerla y, si hace falta ignorar autoimpactos, filtrar por puntero después de resolver el golpe (ver más abajo), no quitando la colisión.
5. Cada tick: `TESObjectREFR::SetPosition`/`SetAngle` actualizan la posición/rotación "lógica", pero **no** el `bhkRigidBody` — hay que escribirlo aparte (`rigidBody->SetPosition(hkVector4)`/`SetRotation(hkQuaternion)`, unidades de Havok vía `bhkWorld::GetWorldScale()`), y llamar `Update3DPosition(true)` al final para sincronizar el nodo visual.

### APIs verificadas para reconstruir lo que daba gratis `RE::Projectile`

Todo esto se comprobó leyendo los headers reales de `commonlibsse-ng` (no asumido de memoria):

- **Colisión (raycast por tick)**: `RE::TES::GetSingleton()->Pick(bhkPickData&)` (`include/RE/T/TES.h`). `bhkPickData` lleva un `hkpWorldRayCastInput` (`from`/`to` en `hkVector4`, `CFilter filterInfo`) y devuelve un `hkpWorldRayCastOutput` (`HasHit()`, `hitFraction`, `normal`, `rootCollidable`). Resolver el `rootCollidable` a una referencia con `RE::TESHavokUtilities::FindCollidableRef(const hkpCollidable&)`.
  - `RE::CFilter` codifica **una sola** capa de colisión (`RE::COL_LAYER`, bits 0-6) — no una máscara de varias, y **no tiene forma de excluir una referencia concreta** (ni al lanzador ni a la propia réplica). Hay que filtrar a mano por puntero después de resolver el impacto (comparar contra el actor lanzador y contra el handle de la propia réplica).
- **Daño**: no existe ningún `ApplyDamage`/`SendHitEvent`/`ProcessHitEvent` expuesto en `commonlibsse-ng` — el pipeline de aplicación real de Bethesda no está expuesto. Sí existen, verificados:
  - `RE::HitData::Populate(Actor* aggressor, Actor* target, InventoryEntryData* weapon, bool isLeftHand)` calcula el daño real (armadura, resistencias, perks, sigilo, críticos) y rellena `hitData.totalDamage`.
  - `RE::InventoryEntryData(TESBoundObject* a_object, std::int32_t a_countDelta)` tiene un constructor público que **no** requiere que el objeto ya esté en un contenedor — se puede construir uno temporal solo para pasarlo a `Populate` (`RE::InventoryEntryData entry(weaponForm, 1);`).
  - `Actor::DamageActorValue(ActorValue::kHealth, hitData.totalDamage)` resta la vida — **se pasa en positivo**, la función internamente hace `-std::abs(valor)`.
  - Con esto se consigue daño numéricamente correcto, pero **sin** reacciones de golpe, tambaleo, números de daño flotantes ni aviso de combate — ese feedback vive en el pipeline interno no expuesto. Limitación aceptada.
- **Arco/velocidad**: `RE::BGSProjectile::data.speed` / `data.gravity` son miembros públicos (`struct BGSProjectileData`) — se pueden leer del formulario Projectile ya configurado en la Creation Kit (`Constants::kThrowableProjectile`) para no inventar una constante de velocidad nueva y mantener lo ya ajustado allí.
- **Agua**: `TESObjectREFR::IsInWater()` existe en la clase base, no solo en `Projectile` — reutilizable en cualquier réplica.
- **Clavado en un actor**: no hay API de "hueso más cercano a un punto del mundo". `NiNode::AttachChild(NiAVObject*, bool)` existe (inverso de `DetachChild`) si se quiere enganchar la réplica al esqueleto del actor, pero localizar el hueso correcto requeriría recorrer el árbol de nodos a mano — sin resolver todavía, ver `Mecanica del arma.txt` para el punto exacto de la mecánica que lo necesita.

### Giro y ángulos de Euler (punto 11) — trampas encontradas

- `TESObjectREFR::SetAngle(NiPoint3{x,y,z})` compone en orden Z-X-Y (`x`=cabeceo/eje X, `y`=alabeo/eje Y, `z`=guiñada/eje Z mundial) — no hay función inversa (`ToEulerAnglesXYZ` descompone en orden X-Y-Z, distinto). Para "mirar hacia una dirección" sin alabeo, calcular cabeceo/guiñada analíticamente (`pitch = -asin(dir.z)`, `yaw = atan2(dir.x, dir.y)`), no por descomposición de matriz.
- **Variar dos canales de Euler a la vez (p. ej. guiñada seguiendo una curva mientras el cabeceo gira rápido) se ve como un sacacorchos, no como un giro limpio** — probado repetidamente en el juego. Solución que funcionó: congelar el canal que no es el del giro (fijarlo al valor inicial) mientras dura el giro rápido, y solo interpolarlo hacia su valor real durante un tramo final de "enderezado" (interpolación angular por el camino más corto, no un lerp lineal ingenuo, para no saltar al cruzar ±π).
- Qué eje usar para el giro (tumbado tipo hacha, no atornillado) es **específico del modelo 3D**, no una fórmula general — para el arma soportada, tras varias rondas de prueba en el juego, lo que se ve bien es un giro constante sobre la guiñada (Z) más un desplazamiento fijo de 90° sobre el alabeo (Y), no girar directamente sobre el cabeceo (X). Verificar de nuevo si se cambia de arma.
- Las cajas de colisión del NIF (`bhkBoxShape` bajo `bhkListShape`, visibles en NifSkope) revelan el eje "adelante" real del modelo con datos, no a ojo: mirar la traslación de cada `bhkTransformShape` (mango vs. cabeza) en vez de asumir que el eje Y local es el eje mango→cabeza.
- La rotación de Havok también necesita sincronizarse a mano, igual que la posición: `bhkRigidBody::SetRotation(hkQuaternion)`, construido desde una `NiMatrix3` (`EulerAnglesToAxesZXY`) vía `NiQuaternion(const NiMatrix3&)`.

### Trampas conocidas (WEAPON / EVENTS)

- `RE::ActorEquipManager::EquipObject`/`UnequipObject` quedan en cola por defecto. Incluso forzando aplicación inmediata (`a_queueEquip=false, a_forceEquip=true, a_applyNow=true`), llamarlo síncronamente dentro de ciertos manejadores de eventos (p. ej. al cerrarse una pantalla de carga) falla en silencio — suena el efecto pero no llega a equipar. Hay que diferirlo un tick con `SKSE::GetTaskInterface()->AddTask`.
- Para detectar que el jugador cambió de celda (puerta, viaje rápido...): `TESCellAttachDetachEvent` nunca se dispara para la referencia del jugador (igual que `OnCellAttach`/`OnCellDetach` en Papyrus); `TESCellFullyLoadedEvent` solo salta si el motor carga datos nuevos, así que falla al volver a una celda ya visitada/en caché. La señal fiable es el cierre de `"Loading Menu"` vía `RE::MenuOpenCloseEvent`/`RE::UI`.

## Source layout

`src/` is organized into numbered topic folders (`1.- CORE`, `2.- INPUT`, `3.- WEAPON`, `4.- THROW`, `5.- RETURN`, `6.- PHYSICS`, `7.- COMBAT`, `8.- ANIMATION`, `9.- MATH`, `10.- EVENTS`, `11.- SKYRIM`). The numbering is just a stable sort order for browsing — it is **not** a build/dependency sequence, so don't assume module N depends only on modules < N.
