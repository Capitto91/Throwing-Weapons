---
name: code-review-effort
description: Antes de ejecutar /code-review (o /security-review) sobre los cambios de este proyecto, o cuando el usuario pregunte si el código está listo para commit/revisión, propone un nivel de esfuerzo (bajo/medio/alto/muy alto/extra) justificado por las zonas de riesgo reales que toca el diff, lo muestra en el encabezado de la respuesta junto con el porqué, y pide confirmación antes de lanzar la revisión — nunca elige el nivel ni la ejecuta en silencio. Fase de calibración semi-manual: úsala siempre que vaya a proponer o lanzar un code review en este repo, hasta que el usuario pida pasar a modo automático.
---

# Elegir el esfuerzo de code review en este proyecto

Este proyecto no tiene un criterio ya fijado para elegir entre `low` / `medium`
/ `high` / `xhigh` / `ultra` al pedir una revisión de código. Esta skill
propone uno basado en qué zonas de riesgo *ya documentadas en `CLAUDE.md`*
toca el diff — no un heurístico genérico de tamaño de cambio — y lo somete a
confirmación del usuario antes de lanzar nada.

**Fase actual: semi-manual.** No autoinvoques `/code-review` con el nivel
elegido sin confirmación previa. `ultra` en concreto no se puede lanzar de
forma autónoma bajo ninguna circunstancia (es de disparo manual y facturado)
— como mucho se recomienda, nunca se ejecuta.

## 1. Determinar el diff a evaluar

`git status` / `git diff` (staged + unstaged) frente a la rama base, o el
rango que el usuario indique. Si no hay cambios, no hay nada que proponer.

## 2. Clasificar qué zonas de riesgo toca

Repasa los archivos y patrones tocados contra esta lista, construida sobre lo
que el propio `CLAUDE.md` del proyecto ya señala como catastrófico si falla:

- **Control manual de física / hilos**: código bajo `4.- THROW`, `5.- RETURN`,
  `6.- PHYSICS`; cualquier `std::thread`, `AddTask`, `bhkRigidBody`,
  `SetMotionType`, `Update3DPosition`, raycasts (`TES::Pick`).
- **Hooks / offsets / multi-runtime**: `REL::Relocation`, `REL::RelocationID`,
  trampolines, cualquier upcast implícito hacia una clase base de `Actor` que
  no sea la primera en su herencia (ver skill `verify-commonlibsse-api`).
- **Serialización / cosave**: `SKSE::SerializationInterface`, `WriteRecord`,
  `Load`.
- **Punteros a forms/referencias de larga duración**: miembros de clase o
  caché estático guardando `Actor*`/`TESObjectREFR*` sin `FormID`/`NiPointer`.
- **Callbacks del motor**: event sinks, hooks, funciones nativas de Papyrus —
  riesgo si pueden lanzar excepciones sin capturar.
- **Módulo o mecánica nueva** (no un ajuste sobre algo ya probado en juego) —
  ver el criterio de versionado `0.Y.Z` del CHANGELOG como señal: si esto
  subiría `Y`, es mecánica nueva.
- **Cambios puramente cosméticos**: comentarios, formato, `CHANGELOG.md`,
  renombrados, ajustes de `.clang-format` — sin tocar lógica.

## 3. Elegir el nivel propuesto

| Nivel | Cuándo |
|---|---|
| **bajo** | Solo cambios cosméticos/documentación, o una función trivial aislada sin ninguna zona de riesgo. |
| **medio** | Lógica nueva o modificada que no toca ninguna zona de riesgo de la lista (p. ej. matemáticas en `9.- MATH`, ajustes de constantes, UI/menús simples). |
| **alto** | El diff toca **una** zona de riesgo de la lista, o implementa una mecánica nueva de alcance moderado. |
| **muy alto** | El diff toca **varias** zonas de riesgo a la vez, o modifica el bucle de control manual tick-a-tick (THROW/RETURN/PHYSICS) o algo con offset multi-runtime. |
| **extra (ultra)** | Cierre de una mecánica completa del documento de diseño, consolidación antes de una release, o el propio usuario indica que va a mergear/publicar. **Solo se recomienda — el usuario debe lanzarlo él mismo.** |

Si dudas entre dos niveles adyacentes, propone el más alto y dilo explícitamente
("dudaba entre medio y alto, propongo alto porque...").

## 4. Presentar la propuesta y pedir confirmación

Antes de invocar la revisión, el **encabezado de tu respuesta** (primera(s)
línea(s), antes de cualquier otro contenido) debe declarar la propuesta en una
línea corta, seguida del porqué en 1-2 frases citando las zonas de riesgo
concretas encontradas (archivos/patrones, no la categoría genérica). Después,
usa `AskUserQuestion` para confirmar — opciones típicas: aceptar el nivel
propuesto, elegir otro nivel de la tabla, o no revisar ahora. No invoques
`/code-review` (vía la skill `code-review`) hasta tener esa confirmación.

Formato de encabezado sugerido:

```
Propuesta de esfuerzo: **alto** — el diff toca ThrowManager.cpp (control manual
de Havok tick-a-tick) y añade un SKSE::GetTaskInterface()->AddTask nuevo.
```

## 5. Tras la confirmación

Ejecuta la revisión con el nivel confirmado (que puede diferir del propuesto,
si el usuario eligió otro). No hace falta repetir la justificación una vez
confirmado.

## Salir de la fase semi-manual

Si el usuario pide explícitamente pasar a modo automático (elegir y lanzar sin
confirmar), no lo hagas por iniciativa propia — es un cambio de alcance de
esta skill y debe pedirse aparte.
