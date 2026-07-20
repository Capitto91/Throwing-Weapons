# Mejoras de KratosCombat aplicables al sistema propio (5 puntos, uno a uno)

## Contexto

`PLAN-proyectil-nativo.md` (raíz del proyecto) documenta 6 ideas extraídas del código real de
KratosCombat que son aplicables a nuestro sistema de réplica manual (decisión ya tomada: nunca
`RE::Projectile`, ver `CLAUDE.md`) sin necesidad de hooks ni trampolines. De esas 6, la nº 6 es solo
una confirmación de un principio que ya seguimos (swap de réplica en vez de reanudar, ya implementado
para el giro) — no requiere código. Este plan cubre las otras 5, para implementarlas **una por una,
probando cada una en el juego antes de pasar a la siguiente**.

Investigado y verificado contra el código real del proyecto y los headers reales de
`commonlibsse-ng` (dos rondas: exploración del estado actual de los archivos afectados, y una
revisión crítica de este mismo diseño) antes de escribir este plan — las notas de riesgo de cada
punto vienen de esa revisión, no son especulación previa a comprobar nada.

**Orden de implementación** (de menor a mayor riesgo, revisado — no es el orden en que aparecen en
`PLAN-proyectil-nativo.md`):

1. Gravedad variable en el lanzamiento (#4)
2. Stagger vía animation graph (#5)
3. Transición de captura al llegar a la mano (#3)
4. Curva de vuelta cúbica + suavizado de dirección (#1+#2, combinados)

El punto 4 se deja para el final a propósito: es el único de los cinco donde un fallo de diseño
podría comprometer la garantía central de la que dependen todos los demás (que el arma vuelva de
verdad a la mano), así que le corresponde el margen de pruebas más amplio, no encajarlo a mitad de
la ronda mientras aún no está resuelto del todo.

Cada punto usa las skills `verify-commonlibsse-api` (antes de dar por buena cualquier firma de
`RE::`/`SKSE::` no verificada ya en este plan) y `skse-plugin-pitfalls` (antes de cerrar cada punto
como terminado) — ya se han aplicado durante el diseño de este plan, pero deben reaplicarse al
escribir el código real de cada punto, no basta con haberlo hecho aquí.

### Protocolo de testeo: campo a campo, no solo visual

Dentro de cada uno de los 4 puntos hay varias constantes/parámetros nuevos (p. ej. en el punto 1:
`kThrowGravityRampDuration`; en el punto 4: `a_anchorFraction`, `kReturnHandApproachOffset`,
`kReturnDirectionBlendWindow`, la red de seguridad por tiempo). No se implementa el punto entero de
golpe para luego probarlo como una caja negra: cada campo/constante nuevo se activa e implementa
**uno a la vez**, en el orden en que aparece descrito dentro del punto, y se prueba en el juego antes
de añadir el siguiente — solo cuando ese campo concreto se confirma correcto se pasa al siguiente
campo del mismo punto, y solo cuando todos los campos de un punto están confirmados se pasa al
punto siguiente.

Además de la confirmación visual en el juego, cada campo debe dejar un log (`logs::info`, alias
`logs::` de `CLAUDE.md`) con su valor/resultado relevante en el momento en que se usa (p. ej. el
valor de `ComputeGravityDrop(elapsed)` calculado, o el vector resuelto de
`handNode->world.rotate.GetVectorY()` antes de confiar en él) — la confirmación de que un campo
funciona no puede depender solo de "se ve bien", tiene que poder verificarse también leyendo el log
de esa sesión de juego.

---

## Punto 1: Gravedad variable en el lanzamiento (`4.- THROW/ThrowManager.cpp`)

**Qué cambia**: la parábola de la ida usa hoy `Constants::kThrowGravity` como constante fija
(`nextPos.z += 0.5f * kThrowGravity * elapsed * elapsed`, forma cerrada, sin acumulación — ver el
comentario ya existente en el archivo sobre por qué se evita deliberadamente acumular tick a tick).
Se sustituye por una rampa lineal de gravedad (0 → `kThrowGravity` durante los primeros
`Constants::kThrowGravityRampDuration` segundos, luego constante), manteniendo la forma cerrada.

**Matemática** (verificada por doble integración, y por diferenciación inversa en la revisión —
posición, velocidad *y* aceleración son continuas en el empalme, sin ningún salto):

- Para `t < T` (`T` = `kThrowGravityRampDuration`): `g(t) = g_max · t/T` →
  `z(t) = g_max · t³ / (6T)`.
- Para `t ≥ T`: continuar como movimiento a gravedad constante desde el estado final de la rampa:
  `z(T) = g_max·T²/6`, `v(T) = g_max·T/2` →
  `z(t) = z(T) + v(T)·(t-T) + 0.5·g_max·(t-T)²`.

**Cómo implementarlo**:

- Nueva función auxiliar en el `namespace` anónimo de `ThrowManager.cpp` (paralela a
  `Return::ComputeTraveledDistance`, no un módulo nuevo — es de uso exclusivo de este archivo):
  `float ComputeGravityDrop(float a_elapsed)`, con las dos ramas de arriba.
  - Guardar `Constants::kThrowGravityRampDuration <= 0.0f` al principio (usar directamente
    `0.5f * kThrowGravity * a_elapsed * a_elapsed`, el comportamiento actual) — mismo criterio que
    ya usa el resto del archivo para divisores degenerados (`ComputeReturnControlPoint`,
    `ComputeReturnAcceleration`).
  - `kThrowGravity` ya es negativo (-1071.816); toda la derivación es lineal en `g`, así que el
    signo se propaga solo — dejar un comentario explícito para que nadie le añada `std::abs` sin
    darse cuenta de que ya está bien.
- Sustituir `nextPos.z += 0.5f * Constants::kThrowGravity * elapsed * elapsed;` por
  `nextPos.z += ComputeGravityDrop(elapsed);`.
- Nueva constante en `Constants.h`: `kThrowGravityRampDuration` (segundos, placeholder — probar en
  el juego, sin valor de referencia previo).

**Verificación en el juego**: lanzar varias veces y confirmar que la trayectoria sale más "plana"/
directa justo al salir de la mano en vez de empezar a caer desde el primer instante, sin ningún
cambio brusco al final de la rampa. Ajustar `kThrowGravityRampDuration` a ojo.

---

## Punto 2: Stagger vía animation graph (`7.- COMBAT/DamageManager.cpp`, `Combat::ApplyReturnHit`)

**Qué cambia**: hoy el empujón al golpear durante el regreso concede un hechizo propio
(`Constants::kStaggerSpell`, `AddSpell`) y lo retira con un hilo que duerme
(`Constants::kStaggerSpellDuration`). Se sustituye por escribir directamente en el animation graph
del actor golpeado — sin hechizo, sin hilo, sin asset de Creation Kit.

**Verificado, no asumido**:

- `IAnimationGraphManagerHolder::SetGraphVariableFloat(const BSFixedString&, float)` y
  `::NotifyAnimationGraph(const BSFixedString&)` existen
  (`lib/commonlibsse-ng/include/RE/I/IAnimationGraphManagerHolder.h:26,53`).
- `RE::Actor` → `RE::TESObjectREFR` (base pública) → `IAnimationGraphManagerHolder` (base pública a
  offset fijo `0x38`, sin variación por runtime, confirmado en `TESObjectREFR.h:104-108`) — **no**
  tiene el mismo problema de offset dependiente de versión que `ActorValueOwner`/`MagicTarget` (ver
  aviso de `CLAUDE.md`); se puede llamar directamente sobre `RE::Actor*` sin ningún accessor
  `AsX()`.
- Los nombres `"staggerMagnitude"`/`"staggerDirection"` **no son una suposición de KratosCombat**:
  están pre-registrados como `BSFixedString` propias del motor en
  `lib/commonlibsse-ng/include/RE/F/FixedStrings.h:167-168` — confirmación independiente de que son
  variables reales del animation graph, no algo inventado por ese mod.
- **Sin verificar** (no lo documenta ningún header ni comentario del proyecto): las unidades/rango
  exacto de `staggerDirection` (¿grados?, ¿radianes?, ¿relativo a qué?). Se empieza con un valor fijo
  `0.0f` ("de frente"), tratado como placeholder explícito — mismo criterio que
  `kThrowInitialSpeed`/`kThrowCollisionRadius` y el resto de constantes sin calibrar todavía en
  `Constants.h`. Calcular la dirección real a partir del vector de impacto es una mejora posterior,
  no parte de esta primera versión.

**Cómo implementarlo**:

- En `Combat::ApplyReturnHit`, sustituir el bloque `AddSpell(kStaggerSpell)` + hilo de retirada por:
  ```cpp
  a_target->SetGraphVariableFloat("staggerMagnitude", Constants::kStaggerMagnitude);
  a_target->SetGraphVariableFloat("staggerDirection", 0.0f);  // placeholder, "de frente"
  a_target->NotifyAnimationGraph("staggerStart");
  ```
- Nueva constante `Constants::kStaggerMagnitude` (float, placeholder). Eliminar
  `Constants::kStaggerSpell` y `Constants::kStaggerSpellDuration` (quedan sin uso).
- El `Ability`/`MagicEffect` de Stagger creados en la Creation Kit para el diseño anterior quedan sin
  uso — no se tocan automáticamente, es decisión del usuario si los limpia o los deja.

**Verificación en el juego**: golpear a un enemigo durante el regreso y confirmar que sigue
tambaleándose (mismo efecto visual que antes), sin necesidad de ningún hechizo concedido/retirado
(comprobable mirando el log o el inventario de hechizos del actor, que ya no debería listar
`kStaggerSpell` en ningún momento).

---

## Punto 3: Transición de captura al llegar a la mano (`3.- WEAPON/WeaponManager.cpp` + nuevo helper)

**Qué cambia**: hoy `WeaponManager::ReequipAndReset` corta en seco — cancela el tick, destruye la
réplica, reequipa el arma real un tick después. Se añade una transición visual breve: clonar el
modelo de la réplica, engancharlo al hueso de la mano un instante, y solo entonces completar el
reequipado — igual que hace KratosCombat al atrapar el hacha.

**Nota sobre el punto 10 del documento de diseño**: el texto literal de `Mecanica del arma.txt`
punto 10 ("se endereza para que... el mango quede orientado para que el jugador pueda agarrarla")
describe una mecánica de **orientación** (usar las cajas de colisión cabeza/mango del NIF) que
**no** es lo mismo que esta transición de captura — son ideas relacionadas pero distintas. Esta
transición es una mejora visual propia (suavizar el corte en el momento del reequipado), no una
implementación del punto 10 tal cual está escrito. El punto 10 en su sentido literal sigue sin
implementarse tras este punto.

**Verificado, no asumido** (revisión dedicada de `NiCloningProcess`/`AttachChild`/lifetime):

- **No construir `NiCloningProcess` a mano.** `NiObject::Clone()` (`NiObject.h:82`) es un wrapper
  público, no virtual, que ya construye internamente el `NiCloningProcess` correcto (relocation real
  al propio motor) — los campos `copyType`/`appendChar` de `NiCloningProcess` no están documentados
  en ningún sitio del proyecto ni de la librería vendorizada (cero usos existentes de construirlo a
  mano en todo `commonlibsse-ng`), así que rellenarlos por adivinación sería una API mal usada.
  Usar `replicaRoot->Clone()` en su lugar.
- `Clone()` devuelve `NiObject*` — usar el mismo idioma de downcast ya usado en este proyecto
  (`WeaponTrail.cpp`, `->AsFadeNode()`), es decir `Clone()->AsNode()`, no `netimmerse_cast` ni
  `static_cast`. Comprobar null en las dos llamadas.
- **Lifetime**: envolver el resultado en `RE::NiPointer` inmediatamente, exactamente como ya hace
  `WeaponTrail.h/.cpp` con su propio efecto (`RE::NiPointer<...>(Spawn(...))`) — mismo patrón ya
  probado en este proyecto, evita tener que resolver de memoria si `Clone()` ya entrega una
  referencia propia o no. `NiNode::AttachChild` añade su propia referencia al array `children` del
  padre, así que tras el `AttachChild` el objeto legítimamente tiene ≥2 dueños — es lo esperado, no
  un fallo.
- **Riesgo nuevo detectado en la revisión, a comprobar**: clonar el nodo raíz clona también
  cualquier `bhkCollisionObject`/cuerpo rígido que lleve el NIF del arma. Ese clon de colisión
  quedaría colgado del hueso de la mano (parte del esqueleto del jugador, activo) sin que nada
  sincronice su posición Havok (no es un `TESObjectREFR`, `Physics::SyncHavok` no aplica aquí) —
  podría quedar una colisión fantasma mal colocada durante la transición. Antes de implementar,
  comprobar en NifSkope si el NIF del arma tiene un nodo de colisión colgado del raíz; si lo tiene,
  clonar el nodo visual hijo concreto en vez del raíz completo, o verificar en el juego que una
  colisión residual de ~0.1-0.3s no causa ningún problema perceptible.
- **Transformación tras `AttachChild`**: el `local` del clon (heredado del original) es relativo a
  la celda, no al hueso de la mano — hay que sobrescribirlo explícitamente (no asumir que el motor
  lo corrige solo al siguiente fotograma), siguiendo el mismo patrón ya usado en
  `WeaponTrail.cpp` (`segmentBone->local = ...; segmentBone->world = ...;`, los dos escritos a
  mano): fijar `local` al offset/rotación fija nueva, y `world` como `handNode->world * offset`,
  justo después del `AttachChild`.
- Sin implicaciones de Havok para el propio clon (aparte del punto de la colisión ya señalado): es
  un `NiAVObject` decorativo, no un `TESObjectREFR` — no le aplica `SetMotionType`/`SyncHavok`.
- **Solo en el camino `onArrived`** (llegada real a la mano), no en `RecallWeapon` (recall
  instantáneo, ya documentado como tal en el propio código; la réplica puede estar lejísimos al
  recular, sin ninguna "captura" real que dramatizar; y se invoca también al cerrar pantallas de
  carga, donde añadir una espera extra antes de que el arma reaparezca se notaría como un bug, no
  como pulido).

**Cómo implementarlo**:

- Nuevo helper, p. ej. `Animation::PlayCaptureTransition(RE::NiAVObject& a_handNode, RE::NiAVObject& a_replicaRoot)`
  en `8.- ANIMATION` (nuevo archivo o añadido a `WeaponAnimation`) — clona, engancha, fija transform,
  y devuelve/gestiona el hilo que lo desengancha tras `Constants::kCaptureTransitionDuration`
  (mismo patrón de hilo-que-duerme-y-reencola ya usado en todo el proyecto).
- Refactorizar `WeaponManager::ReequipAndReset` en dos partes — "destruir réplica y limpiar estado"
  (inmediato) y "reequipar arma real" (ya diferido) — para que el camino `onArrived` pueda insertar
  la transición de captura *entre* ambas sin duplicar la lógica de equipar, mientras `RecallWeapon`
  sigue usando el camino actual sin cambios.
- Importante: clonar la réplica **antes** de `Physics::DestroyReplica` (que llama a `Disable()`) —
  no clonar después de deshabilitarla.
- Nuevas constantes en `Constants.h`: `kCaptureTransitionLocalOffset` (`NiPoint3`),
  `kCaptureTransitionLocalRotation` (si hace falta, o reutilizar una rotación fija simple),
  `kCaptureTransitionDuration` (segundos) — todo placeholder, a ajustar en el juego.

**Verificación en el juego**: recuperar el arma dejando que llegue de verdad a la mano (no recall
instantáneo) y confirmar que se ve una transición breve en vez de un corte seco, sin ningún modelo
fantasma que se quede colgado más de lo previsto ni ninguna colisión rara notable durante ese
instante. Probar también el camino `RecallWeapon` (botón de recall a mitad de vuelo) y confirmar que
sigue siendo instantáneo, sin transición, como hoy.

---

## Punto 4: Curva de vuelta cúbica + suavizado de dirección (`5.- RETURN/ReturnTrajectory.*`, `ReturnManager.cpp`)

**Qué cambia**: hoy el regreso usa una Bézier cuadrática (un único punto de control, calculado una
vez al empezar, desplazamiento lateral respecto a la línea recta) y cada tick salta directamente a
la posición exacta sobre la curva. Se sustituye por una Bézier cúbica (dos puntos de control, el
segundo orientado hacia la mano y recalculado cada tick) más un suavizado de la dirección de
movimiento en vez de saltar directo a la posición exacta — decisión ya tomada con el usuario de
implementarlo con las mitigaciones de abajo, pese al riesgo detectado en la revisión.

**Riesgo de fondo, explícito**: la fórmula actual es cerrada (posición exacta cada tick,
autocorregible sin memoria del pasado) — el mismo principio que `Throw::LaunchWeapon` aplica a
propósito ("forma cerrada... para no arrastrar deriva numérica"). Suavizar la dirección introduce
**acumulación** tick a tick (se integra una dirección mezclada en vez de saltar a un punto exacto),
lo que rompe esa propiedad autocorrectora. El riesgo concreto: si el jugador gira/se mueve mucho
justo en el tramo final, la dirección (con retraso de la ventana de suavizado) puede perseguir un
objetivo que ya se movió, con la velocidad más alta de todo el trayecto (aceleración constante desde
el reposo) justo ahí — posible sobrepaso u oscilación cerca de la llegada. Mitigado con las medidas
de abajo, pero **hay que probarlo específicamente girando/moviéndose durante el tramo final del
regreso**, no basta con probar en quieto.

**Puntos de control**:

- `p0` = inicio (sin cambios).
- `p1` = la función existente `ComputeReturnControlPoint` (`ReturnTrajectory.cpp`), **con un
  parámetro nuevo** `a_anchorFraction` (no una función nueva — el punto de anclaje a lo largo de la
  línea inicio→mano está hoy fijo al punto medio dentro del cuerpo de la función, hace falta
  parametrizarlo). Con `a_anchorFraction = 1/3` en vez del `0.5` implícito de hoy, para que el primer
  punto de control quede cerca del origen (como en Kratos) en vez de en el medio. Los llamantes
  existentes (si los hay fuera de este punto) usarían `0.5f` para no cambiar de comportamiento.
  - **Aviso importante**: esto es un concepto distinto de `kReturnCurveLateralFractionMin/Max`
    (que controlan la *magnitud* del desplazamiento lateral, ya aleatorizada) — dejar un comentario
    claro distinguiendo "fracción de anclaje a lo largo de la línea" de "fracción de desplazamiento
    lateral" para que no se confundan en el futuro.
- `p2` = **nuevo**, recalculado cada tick (a diferencia de `p1`, fijado una vez): cerca de la mano
  actual, desplazado hacia atrás a lo largo del eje "adelante" del propio hueso de la mano
  (`handNode->world.rotate.GetVectorY()`, por analogía con la convención ya usada en
  `ThrowManager::GetCameraForward`) una distancia `Constants::kReturnHandApproachOffset`.
  - **Sin verificar, riesgo señalado en la revisión**: no hay precedente en este proyecto de leer la
    *rotación* del hueso `"WEAPON"` (todo el código existente solo lee su `world.translate`) — a
    diferencia de la cámara, un hueso de esqueleto puede tener ejes locales rotados/arbitrarios sin
    relación clara con "hacia la palma". **Antes de confiar en este vector**, comprobar en el juego
    (log del vector, o un marcador visual temporal) qué eje/signo apunta de verdad "hacia fuera de
    la palma" — no asumir que Y es correcto por analogía con la cámara.
- `p3` = posición actual de la mano (recalculada cada tick, sin cambios respecto a hoy).

**Suavizado de dirección — con las mitigaciones acordadas**:

- Nueva función `Math::EvaluateCubicBezier` en `9.- MATH/CurveMath.h/.cpp` (fórmula estándar,
  mecánica, mismo estilo que `EvaluateQuadraticBezier` ya existente).
- Nueva función `Math::BlendDirections(const NiPoint3& a_previous, const NiPoint3& a_target, float a_factor)`
  (lerp + normalizar) — **con protección ante vector degenerado** (si el resultado del lerp es
  casi cero, p. ej. direcciones casi opuestas, caer directamente a `a_target` normalizado en vez de
  normalizar un vector casi nulo) — mismo criterio que ya usa `ComputeReturnControlPoint` con su
  cadena de fallback (perpendicular → Z del mundo → X del mundo).
- Nuevo estado mutable en la lambda del tick de `Return::BeginReturn` (mismo patrón que
  `elapsed`/`hitActors`): dirección del tick anterior — **sembrarla explícitamente en el primer
  tick** (no hay tick anterior todavía; usar la dirección cruda del primer tick como semilla, sin
  intentar mezclar con nada).
- Cada tick: calcular la posición cruda sobre la curva cúbica, derivar de ahí una dirección deseada,
  mezclarla con `Math::BlendDirections` usando `Constants::kReturnDirectionBlendWindow` (segundos,
  placeholder ~0.2s, igual que Kratos) como factor de mezcla, y avanzar la posición real a lo largo
  de la dirección mezclada, a la velocidad que ya implica la cinemática existente (delta de
  `ComputeTraveledDistance` entre este tick y el anterior) — no una velocidad nueva inventada.
- **Red de seguridad obligatoria**: además de la condición de llegada por distancia ya existente
  (`(handPos - nextPos).Length() <= kReturnArrivalDistance`), añadir una llegada forzada por tiempo
  transcurrido (mismo papel que `kReturnMaxDuration` ya cumple con la velocidad) — si el suavizado
  no converge a tiempo por el motivo que sea, el regreso no debe poder quedarse enganchado
  indefinidamente.

**Verificación en el juego**: repetir el regreso en quieto (confirmar que la curva se sigue viendo
bien, entrando a la mano desde un ángulo más natural que antes) y, específicamente, **girando o
moviéndose de forma notable durante el tramo final del regreso** — confirmar que el arma sigue
llegando a la mano sin quedarse dando vueltas, sin sobrepasar visiblemente el punto de llegada, y
sin nunca superar el tiempo de la red de seguridad.
