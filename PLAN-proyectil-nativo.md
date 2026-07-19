# Mejoras de KratosCombat aplicables a nuestro sistema (sin migrar a `RE::Projectile`)

**Estado: documento de referencia, no implementación.** Decisión ya tomada: este proyecto sigue con
su propia réplica controlada a mano (nunca `RE::Projectile`) — ver `CLAUDE.md`, "Arquitectura de
física de proyectiles". Migrar a un `RE::Projectile` nativo al estilo KratosCombat exigiría adoptar
hooks de vtabla y trampolines sobre direcciones internas del motor — un salto de riesgo de
compatibilidad (choque con otros mods que toquen las mismas vtablas/direcciones) y de mantenimiento
(fragilidad entre versiones del juego) que no se considera justificado. Este documento ya no evalúa
esa migración: describe cómo funciona KratosCombat en general y extrae qué partes de su diseño **sí**
se pueden trasladar a nuestro sistema manual sin asumir ese riesgo.

## Cómo funciona KratosCombat (Leviathan Axe), en general

Investigación completa sobre el código fuente real (`github.com/PhiloSocio/KratosCombat`,
`src/MainKratosCombat.cpp`, `src/hook.cpp`, `src/hook.h`, `src/util.h`). Resumen de su arquitectura:

- **Ida y vuelta son dos `RE::Projectile` nativos independientes** (`RE::Projectile::Launch`), no un
  mismo objeto reciclado ni una réplica manual. Al capturar el hacha, el proyectil de vuelta se
  marca destruido y se reequipa el arma real directamente.
- **La trayectoria (homing de ida, curva de vuelta) se calcula dentro de un hook de vtabla**
  (`GetLinearVelocity`, vfunc `0x86` de `ArrowProjectile`/`MissileProjectile`) que el propio motor
  llama una vez por fotograma como parte de su física nativa — no usan ningún hilo ni bucle propio.
  - La curva de vuelta es una **Bézier cúbica real**: `p0` = posición donde empezó la vuelta, `p3` =
    mano del jugador (recalculada cada frame), `p1` = un tercio en la dirección de salida, `p2` =
    cerca de la mano, orientado según la dirección de la palma y la orientación de la mano
    (`palmDir`, `handForward`). El resultado no se aplica como una posición directa: se convierte en
    una dirección deseada que se **mezcla con la velocidad del frame anterior** a lo largo de una
    ventana de 0.2s (`MathUtil::Angle::BlendVectors`), para no dar un tirón visual de golpe.
  - La orientación visual en vuelo (`data.angle`) se recalcula en el mismo sitio — no usan
    `NiTimeController` activo para esto (la función que sí manipula una matriz de rotación a mano,
    `SetHitRotation`, existe pero está desactivada en el repo, `#ifdef EXPERIMENTAL` sin definir).
  - Mutan `BGSProjectile::data.gravity` en tiempo real: gravedad casi nula al empezar el vuelo,
    reintroducida gradualmente — un ajuste de "feel" para que el lanzamiento se note más directo al
    principio en vez de empezar a caer desde el primer instante.
- **La colisión se resuelve interceptando la Havok nativa**, no con raycasts propios: hookean
  `GetCollisionArrow`/`GetCollisionMissile` (vfunc `0xBE`) y `GetArrowImpactData`/
  `GetMissileImpactData` (vfunc `0xBD`), y deciden `ImpactResult::kStick` (clavado) o `kBounce`
  (rebote) reutilizando el mismo mecanismo que las flechas vanilla.
- **El daño lo aplica el pipeline nativo del juego** (por ser un impacto de un `Projectile` real
  procesado por el motor) — solo *escalan* el importe (`HitData::totalDamage`/`weaponDamage`) antes
  del golpe, interceptando una función interna del motor con un trampolín (`RELOCATION_ID` +
  parcheo de una instrucción `call` de 5 bytes), no vía ninguna API documentada.
- **No hay parálisis ni control de masas al clavarse.** El "clavado" es solo `ImpactResult::kStick`
  nativo, sin ningún hechizo de por medio. Para el stagger (en su lanza Draupnir, no en el hacha),
  usan una utilidad (`FenixUtils::stagger`, `src/util.h`) que escribe directamente variables del
  *animation graph* del actor (`SetGraphVariableFloat("staggerDirection"/"staggerMagnitude")` +
  `NotifyAnimationGraph("staggerStart")`) — sin crear ni conceder ningún hechizo.
- **La vuelta incluye una transición visual antes del reequipado real**: clonan el modelo del arma
  equipada (`weaponModelCopy->Clone()`) y lo cuelgan de un hueso del propio NIF del proyectil durante
  el vuelo; al atrapar, ese clon se reengancha un instante al hueso de la mano (con una corrección de
  rotación fija) antes de reequipar el arma real (`EquipItem(..., skipEquipAnim)`) y destruir el
  clon.

## Qué de esto se puede aplicar a nuestro sistema, sin proyectil nativo ni hooks

Todo lo siguiente es matemática o manipulación de nodos aplicada a **nuestra propia** réplica
controlada a mano — ningún hook de vtabla, ningún trampolín, ninguna dependencia de `RE::Projectile`.

### 1. Suavizar la dirección de la curva de vuelta (`BlendVectors`)

Hoy (`Return::BeginReturnTrajectory`, `5.- RETURN/ReturnManager.cpp`) cada tick salta directamente a
la posición exacta sobre la Bézier (`a_refr.SetPosition(nextPos)`). Si el jugador gira de golpe
mientras el arma vuelve, el extremo de la curva (la mano) se mueve y el arma puede dar un tirón
visual brusco.

Kratos, en vez de saltar directo a la posición de la curva, calcula una dirección deseada (tangente
de la curva) y la mezcla con la dirección/velocidad del tick anterior a lo largo de una ventana de
tiempo corta, así el cambio de rumbo se suaviza en vez de ser instantáneo.

Aplicable directamente: introducir un suavizado equivalente (interpolar la dirección de movimiento
con un factor de tiempo) antes de aplicar `SetPosition` cada tick, en vez de seguir la curva de
forma perfectamente rígida.

### 2. Puntos de control de la curva orientados a la mano, no solo laterales

Hoy `Return::ComputeReturnControlPoint` (`5.- RETURN/ReturnTrajectory.cpp`) usa un único punto de
control (Bézier **cuadrática**) situado lateralmente respecto a la línea recta inicio→mano, con el
vector "derecha" del jugador (`GetPlayerRightVector`), fijado una única vez al empezar el regreso.

Kratos usa una Bézier **cúbica** (dos puntos de control): uno cerca del origen (a lo largo de la
dirección de salida), y uno cerca de la mano, orientado según la palma y el "hacia delante" de la
propia mano — el resultado es que el arma no solo describe un arco lateral, sino que además *entra*
a la mano desde un ángulo que parece un enganche natural, no solo una curva genérica.

Aplicable como mejora de diseño (no trivial, cambia el grado de la curva): pasar de Bézier cuadrática
a cúbica, con el segundo punto de control calculado a partir de la orientación del hueso de la mano
en vez de un valor fijo.

### 3. Transición de "captura" — clon reenganchado a la mano antes de reequipar

Esto llena un hueco real de nuestro propio diseño: el punto 10 de `Mecanica del arma.txt` pide que el
arma "se endereza... justo antes de volver a la mano", y hoy no está implementado —
`WeaponManager::ReequipAndReset` simplemente destruye la réplica y reequipa, sin ninguna transición.

Kratos lo resuelve clonando el modelo visual y reenganchándolo un instante al hueso de la mano
(`AttachChild` con una corrección de rotación fija) justo antes del reequipado real, para que no haya
un corte brusco entre "réplica volando" y "arma ya en la mano". Es la misma familia de técnica que ya
usamos en `8.- ANIMATION/WeaponTrail` (manipulación de nodos NiNode) — cero riesgo de motor, y
directamente relevante para completar el punto 10 sin depender del `NiTransformController` que ya
hemos demostrado que falla.

### 4. Gravedad variable en el lanzamiento (en vez de constante)

Hoy `Throw::LaunchWeapon` usa `Constants::kThrowGravity` como constante fija en toda la trayectoria
(`nextPos.z += 0.5f * Constants::kThrowGravity * elapsed * elapsed`).

Kratos muta la gravedad del proyectil en tiempo real: casi nula al principio del vuelo, reintroducida
gradualmente — el lanzamiento se nota más "directo"/plano al salir de la mano en vez de empezar a
caer desde el primer instante.

Aplicable directamente a nuestra propia fórmula: sustituir la gravedad constante por una función del
tiempo transcurrido (p. ej. una rampa de 0 a `kThrowGravity` durante los primeros N milisegundos),
sin ninguna dependencia de `BGSProjectile` ni de Havok — es nuestra propia fórmula, ya la controlamos
por completo.

### 5. Stagger vía animation graph en vez de hechizo (posible simplificación del punto 9)

Hoy (`Combat::ApplyReturnHit`, `7.- COMBAT/DamageManager.cpp`) el empujón al golpear durante el
regreso se consigue concediendo un hechizo propio (`Constants::kStaggerSpell`, `AddSpell`) y
retirándolo poco después con un hilo que duerme.

Kratos consigue el mismo efecto sin ningún hechizo: escribe directamente variables del *animation
graph* del actor golpeado (`SetGraphVariableFloat("staggerDirection", ...)`,
`SetGraphVariableFloat("staggerMagnitude", ...)`, `NotifyAnimationGraph("staggerStart")`) — el
tambaleo se dispara al instante, sin crear ni gestionar el ciclo de vida de un hechizo, y sin
necesidad de ningún asset nuevo en la Creation Kit.

No es una funcionalidad nueva (nuestro punto 9 ya funciona), pero sí una posible simplificación real:
menos piezas moviéndose (sin `AddSpell`/`RemoveSpell`/hilo de temporización), a cambio de depender de
que `Actor::SetGraphVariableFloat`/`NotifyAnimationGraph` existan como API pública verificada en
`commonlibsse-ng` (pendiente de confirmar antes de implementarlo, ver más abajo — no darlo por bueno
solo porque Kratos lo usa).

### 6. Confirmación de un principio que ya adoptamos: nunca reanudar, crear una instancia nueva

Kratos nunca reutiliza el mismo `Projectile` entre ida y vuelta — lanza uno nuevo cada vez. Es el
mismo principio que ya aplicamos en el rediseño del giro (`Return::BeginReturn`, swap de réplica al
volver desde `kStuck`): no intentar reanudar el estado de un objeto que ya pasó por una fase
distinta, crear uno fresco. No es una idea nueva que adoptar, pero confirma que el criterio que ya
seguimos coincide con el de un mod publicado y probado en el mundo real.

## Qué se queda fuera, y por qué

Estas piezas del diseño de Kratos dependen de tener un `RE::Projectile` real y de hooks de vtabla o
trampolines, así que quedan fuera de alcance con la decisión ya tomada de no migrar:

- Colisión resuelta por Havok nativo (en vez de nuestro `Collision::SweepRaycast`).
- Daño y feedback de combate del pipeline nativo (reacciones de golpe, números flotantes, aviso a
  IA) — seguimos con nuestro cálculo manual (`HitData::Populate` + `DealDamage` revertido).
- El giro/orientación en vuelo enganchado a un tick garantizado por fotograma — seguimos dependiendo
  de nuestro hilo propio (`Physics::StartTickLoop`), con el margen de error que ya conocemos.

## Próximos pasos, si se decide implementar alguna de estas mejoras

Cada una es independiente de las demás — se pueden priorizar/implementar por separado:

1. **Suavizado de dirección (#1)** y **puntos de control orientados a la mano (#2)**: ambas tocan
   `5.- RETURN/ReturnManager.cpp` / `5.- RETURN/ReturnTrajectory.cpp` — conviene diseñarlas juntas,
   ya que la segunda cambia el grado de la curva (de cuadrática a cúbica) y la primera cambia cómo se
   aplica el resultado cada tick.
2. **Transición de captura (#3)**: nuevo comportamiento en `WeaponManager::ReequipAndReset`
   (`3.- WEAPON/WeaponManager.cpp`) más un helper nuevo en `8.- ANIMATION` para clonar/reenganchar el
   nodo — antes de implementar, consultar `Mecanica del arma.txt` punto 10 (el propio documento de
   diseño, no solo esta nota) porque toca directamente esa mecánica.
3. **Gravedad variable en el lanzamiento (#4)**: cambio contenido en `4.- THROW/ThrowManager.cpp` +
   una constante nueva en `Constants.h` (p. ej. `kThrowGravityRampDuration`).
4. **Stagger vía animation graph (#5)**: antes de tocar `Combat::ApplyReturnHit`, verificar contra los
   headers reales de `commonlibsse-ng` que `Actor::SetGraphVariableFloat`/`NotifyAnimationGraph`
   existen con esa firma exacta — mismo criterio que ya pide `CLAUDE.md` para cualquier API.
