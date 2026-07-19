---
name: verify-commonlibsse-api
description: Verifica contra los headers reales de lib/commonlibsse-ng la firma exacta, offset y seguridad multi-runtime de cualquier clase/función/miembro de RE:: antes de usarla en código nuevo o de dar por buena una que ya está en el código. Úsala siempre que vayas a escribir o revisar una llamada a una API de CommonLibSSE-NG (RE::, SKSE::) que no hayas verificado ya en esta misma conversación, incluso si el nombre "suena" correcto o lo recuerdas de otro proyecto — la firma real puede diferir entre forks (Sample/Fudge/NG) y el fork vendido aquí es siempre la única fuente de verdad.
---

# Verificación de APIs de CommonLibSSE-NG contra los headers reales

Este proyecto compila contra el submódulo `lib/commonlibsse-ng` (CommonLibVR,
rama `ng`). Ese directorio es la **única** fuente de verdad para cualquier
firma de `RE::`/`SKSE::` — no la memoria de entrenamiento, que mezcla varias
forks (CommonLibSSE original "Sample", Fudge, NG) con diferencias reales entre
ellas. Antes de escribir o aceptar una llamada a una API que no hayas
verificado ya en esta conversación, sigue este proceso.

Si `lib/commonlibsse-ng` está vacío o falta, dilo explícitamente y pide
`git submodule update --init --recursive` antes de continuar — no sigas
verificando de memoria como sustituto.

## 1. Localizar la clase/función

Los headers de `RE::` están organizados por letra inicial de la clase:
`lib/commonlibsse-ng/include/RE/<Letra>/<Clase>.h` (p. ej.
`RE::TESObjectREFR` → `include/RE/T/TESObjectREFR.h`). Las de `SKSE::` están
bajo `lib/commonlibsse-ng/include/SKSE/`.

Si no sabes el nombre exacto de la clase, usa Grep sobre
`lib/commonlibsse-ng/include` en vez de adivinar el path — el nombre puede no
coincidir con lo que recuerdas (renombrados entre forks, namespaces internos
como `RE::BSScript::` o `RE::Havok`, etc.).

## 2. Confirmar la firma exacta

Lee el bloque de la clase y localiza el miembro exacto: nombre, tipo de
retorno, parámetros y su orden, `const`/no-const, y si es virtual. No asumas
que un nombre de método común (`GetX`, `SetX`, `Update3DPosition`...) tiene la
misma firma que en otro proyecto o versión — cítala tal cual aparece en el
header, con `archivo:línea`.

Si el método no aparece en la clase que esperabas, comprueba si viene de una
clase base (revisa la lista de herencia en la declaración de la clase) antes
de descartarlo.

## 3. Comprobar el riesgo de offset por herencia múltiple

Este es el error más caro y menos visible en compilación (ver
`CLAUDE.md` → "Herencia múltiple de `RE::Actor` con offset distinto según
versión del juego" para el caso ya confirmado con `ActorValueOwner`).

Si el miembro que vas a llamar **no** está declarado directamente en la clase
sobre la que tienes el puntero, sino en una de sus clases base, y esa base
**no es la primera** en la lista de herencia:

- Busca en el header si existe un accessor versionado — patrón
  `RUNTIME_CAST_ACCESSOR_VERSIONED` (grep ese literal en el header de la
  clase). Si existe (p. ej. `Actor::AsActorValueOwner()`,
  `Actor::AsMagicTarget()`), es obligatorio usarlo en vez de un upcast
  implícito del puntero — el proyecto compila multi-runtime
  (`SKYRIM_CROSS_VR`, ver `xmake.lua`) y el compilador solo puede fijar un
  offset en tiempo de compilación, que puede no coincidir con la versión real
  del juego en ejecución.
- Si no existe accessor versionado y la clase base no es la primera en la
  lista de herencia, señala el riesgo explícitamente al usuario en vez de
  usar el upcast implícito sin más — puede que simplemente no se haya dado
  aún el caso, no que sea seguro.
- Los métodos declarados directamente en la propia clase (offset 0, p. ej.
  `Actor::AddSpell`) no tienen este problema.

## 4. Comprobar otras trampas conocidas de la API en cuestión

Repasa si el símbolo encontrado coincide con alguna de las categorías ya
documentadas en `CLAUDE.md` (sección "Arquitectura de física de proyectiles" y
"Errores comunes a vigilar"): direcciones hardcodeadas en vez de
`REL::RelocationID`, punteros a forms/referencias sin comprobar nulo, llamadas
que mutan el juego fuera del hilo principal, excepciones que puedan cruzar un
callback del motor. Si detectas alguna, indícalo junto con la firma
verificada, no por separado.

## 5. Reportar el resultado

Para cada API verificada:

```
### `RE::Clase::metodo(...)`
**Firma real:** tal cual aparece en el header, con tipos completos
**Fuente:** lib/commonlibsse-ng/include/RE/X/Clase.h:línea
**Offset/herencia:** directo en la clase | heredado de <Base> (primera base / no primera → accessor versionado necesario: sí/no)
**Otras trampas:** ninguna | lista breve
```

Si no encuentras el símbolo en absoluto tras buscar variantes razonables de
nombre, dilo explícitamente — "no encontrado en commonlibsse-ng, no lo uses
tal cual" — en vez de dar por buena una firma recordada de memoria. Esto
aplica el mismo criterio que ya pide el CLAUDE.md del proyecto para el
documento de diseño: si no se puede verificar, decirlo en vez de asumir.
