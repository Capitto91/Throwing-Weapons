# Plan: animaciones de apuntar/lanzar/recuperar (placeholder)

Estado: **pendiente de implementar**. Este archivo es autocontenido — no
hace falta leer nada más para retomarlo, aunque hay más detalle histórico
(investigación de mods de referencia, capturas de NifSkope de otra tarea ya
cerrada) en `C:\Users\bbarc\.claude\plans\stateful-napping-origami.md` si
hiciera falta.

## Contexto

Actualmente `WeaponManager::ThrowWeapon` desequipa el arma real de forma
síncrona y sin ninguna señal de animación; `ReequipAndReset`/`RecoverOrReset`
la reequipan igual de síncrono (diferido un tick solo por el bug de motor ya
documentado en `CLAUDE.md`). Como Skyrim reacciona a un desequipar/equipar
real cambiando la pose del personaje (a puños, y de vuelta), el jugador
percibe un "salto"/animación de envainar-desenvainar no deseada, sin
relación con el lanzamiento o la recuperación real del arma.

El usuario quiere: botón de apuntar → animación de apuntar (mantenida
mientras se apunta); soltar → animación de lanzar; botón de recuperar →
animación de recuperar. **No hay animaciones reales todavía** — placeholders
(solo cableado, sin pedir prestada ninguna animación vanilla — decisión ya
tomada con el usuario, no reabrir).

## Decisiones ya tomadas con el usuario (no reabrir sin motivo)

- Placeholder = solo `Actor::NotifyAnimationGraph` con nombres de evento
  inventados, sin efecto visual hasta que existan animaciones reales — no
  pedir prestada ninguna animación vanilla como sustituto.
- Límite de continuidad de pose **aceptado**: el nodo `WEAPON` es parte del
  esqueleto base, no del arma — su posición depende de la pose actual del
  personaje. Al desequipar de verdad (necesario para el punto 4 de
  `Mecanica del arma.txt`: pelea a puños), la pose cambia a manos vacías,
  distinta de sujetar el arma. El punto exacto donde aparece/desaparece la
  réplica nunca va a coincidir a la perfección hasta que haya animaciones de
  verdad — aceptado, no es un bug a perseguir.
- Aplazado originalmente para dar prioridad al giro (punto 10) — **ya
  resuelto** (NIF + `8.- ANIMATION/WeaponAnimation`, ver `CHANGELOG.md`
  v1.6.1-v1.6.3).

## Investigación ya verificada contra los headers reales (no repetir)

- `Actor::NotifyAnimationGraph(const BSFixedString&)` — confirmado, vía
  `IAnimationGraphManagerHolder`.
- `ActorEquipManager::EquipObject`/`UnequipObject` no tienen ningún flag
  para suprimir la animación de equipar/desequipar (solo `a_playSounds`,
  que es sonido, no animación).
- `Actor::OnItemEquipped(bool a_playAnim)` (`Actor.h:391`, slot de vtable
  `0xB2`, `SKYRIM_REL_VR_VIRTUAL`) — la función nativa que el motor consulta
  para decidir si reproduce la animación de envainar/desenvainar. Vía real
  para suprimirla sin Nemesis (encontrada investigando el mod de referencia
  `SkipEquipAnimation`, que la hookea con `write_vfunc` y fuerza
  `a_playAnim = false`). **Pendiente de verificar antes de usarla**: técnica
  nueva para este proyecto (nunca hemos hookeado un método virtual de un
  singleton del motor, solo funciones sueltas vía `REL::Relocation`, ver
  `1.- CORE/GameOffsets.h`) — `SKYRIM_REL_VR_VIRTUAL` ya resuelve *llamar* a
  la función correctamente entre SE/AE/VR, pero *sobrescribir* el puntero
  (`write_vfunc`) habría que verificarlo aparte, podría desplazarse entre
  runtimes.

## Qué ha cambiado desde que se aparcó este plan (repasar antes de implementar)

- **Ya existe cosave** (`CHANGELOG.md` v1.6.5-v1.6.6):
  `WeaponManager::SaveCycleData`/`CaptureSaveData()`/`RecoverOrReset()`,
  registrado en `kPostLoad`/`kPostLoadGame`. Si esta tarea añade algún
  estado transitorio nuevo (p. ej. una ventana de "lanzando" antes de que
  el desequipar real ocurra), hay que decidir explícitamente si cuenta como
  `cycleActive` para el cosave — probablemente no (nada físico ha cambiado
  todavía en esa ventana: el arma sigue equipada, no existe réplica), pero
  repasarlo al diseñar el estado nuevo, no darlo por hecho.
- **`8.- ANIMATION/WeaponAnimation` ya tiene contenido real**: `StartSpin`/
  `StopSpin` para el giro de la *réplica* (horneado en el NIF, no
  relacionado con esto). Las animaciones de *cuerpo del jugador* que pide
  esta tarea son un concern distinto — decidir en el diseño si comparten
  namespace/archivo (con nombres claramente distintos) o merecen uno propio.
- El clavado en actor ahora seguí un hueso concreto
  (`ActorUtils::FindNearestBoneName`, v1.6.3) — no afecta a esta tarea.

## Estados y puntos de enganche ya existentes (reutilizar, no rehacer)

`Weapon::WeaponState`/`WeaponManager` (`3.- WEAPON/`) ya tiene la máquina de
estados (`kInHand`/`kAiming`/`kThrown`/`kStuck`/`kReturning`) y los puntos
exactos donde engancharían las llamadas a animación: `BeginAiming`
(apuntar), `ThrowWeapon` (lanzar), `BeginReturn` (recuperar) y
`ReequipAndReset`/`RecoverOrReset` (llegada). `OnLoadingScreenClosed` ya
interrumpe el ciclo a mitad de cualquier fase — cualquier estado nuevo
(p. ej. una ventana de "lanzando" antes del desequipar real) tendría que
cubrirse ahí también.

## Diseño pendiente de decidir/implementar

1. **Placeholder confirmado**: solo cableado con `NotifyAnimationGraph`.
2. Diseñar la ventana de tiempo (constante placeholder en `Constants.h`)
   entre "se suelta el botón de lanzar" y "el desequipar/spawn de la réplica
   ocurre de verdad", para acercar el salto de pose al instante en que la
   réplica aparece — y el equivalente al recuperar.
3. Cubrir los casos límite: pulsar de nuevo durante la ventana de
   lanzamiento (antes de que exista réplica), y `OnLoadingScreenClosed` a
   mitad de esa ventana.
4. **Decidir el alcance de `Actor::OnItemEquipped`**: ¿se prueba ya el hook
   de vtable como sustituto (parcial o total) de la sincronización de
   ventana, o se implementa primero la vía simple (solo ventana de tiempo,
   sin suprimir la animación nativa) y se deja el hook como mejora
   posterior? Recomendación: empezar por la vía simple — es la que no
   introduce una técnica nueva sin verificar, y ya resuelve el problema
   original (aunque sin eliminar la animación nativa del todo).

## Próximos pasos concretos al retomar

1. Confirmar con el usuario si sigue en pie la recomendación del punto 4
   (empezar simple, sin el hook de vtable).
2. Diseñar los 3 nuevos puntos de enganche de animación (apuntar/lanzar/
   recuperar) en `WeaponManager`, mismo patrón que ya usa el ciclo actual.
3. Entrar en plan mode para finalizar el diseño concreto antes de tocar
   código, dado el tamaño del cambio (toca `WeaponManager`, `WeaponState`,
   `Constants.h`, y un módulo de animación de personaje nuevo o ampliado).

## Verificación

1. Compilar.
2. Apuntar, lanzar y recuperar el arma en el juego; confirmar en el log que
   los eventos placeholder se envían en el momento correcto de cada fase
   (sin esperar efecto visual todavía, no hay animaciones reales).
3. Probar los casos límite: pulsar de nuevo durante la ventana de
   lanzamiento, y cerrar una pantalla de carga a mitad de esa ventana.
4. Si se implementa el hook de `OnItemEquipped`: confirmar en el juego que
   la animación nativa de envainar/desenvainar deja de reproducirse, y que
   sigue funcionando igual en SE/AE/VR si hay forma de probarlo.
