# Changelog

Registro de cambios relevantes del plugin, en español. Versión `0.Y.Z`: `Y` sube al implementar una mecánica nueva (y `Z` vuelve a 1), `Z` sube al modificar una mecánica ya existente.

## 2026-07-12

### v0.0.1

- Sistema de entrada (`InputManager`):
  - Detección del botón de apuntar/lanzar/recuperar, configurable por INI (`Data/SKSE/Plugins/ThrowingWeapons.ini`, por defecto tecla R).
  - Restringido al arma arrojadiza única, identificada mediante una Keyword (`WAF_ThrowableWeapon`) equipada en la mano derecha.

### v0.1.1

- Ciclo de vida del arma (`WeaponState`/`WeaponManager`):
  - Máquina de estados: en mano / apuntando / lanzada / clavada / regresando.
  - El mismo botón apunta+lanza o recupera el arma, según el estado actual.
  - Lanzar/recuperar desequipa y reequipa el arma original (mano vacía → combate a puños real, según la mecánica). Transición instantánea por ahora; trayectoria y proyectil réplica quedan pendientes de `THROW`/`RETURN`/`PHYSICS`.

### v0.1.2

- Robustez del ciclo de vida del arma (`WeaponManager`/`EventManager`):
  - El arma comprometida con el ciclo se fija al empezar a apuntar (no al soltar el botón), para no lanzar por error un arma distinta si el jugador la cambia a mitad de apuntado.
  - Resincronización con la partida real al cargar/empezar (`kPostLoadGame`/`kNewGame`): el estado vuelve siempre a "en mano", evitando quedar desincronizado si se guardó con el arma fuera.
  - Autocorrección si se pierde el botón de soltar (p. ej. pantalla de carga a mitad de pulsación): una pulsación nueva estando ya "apuntando" reinicia el ciclo en vez de quedarse atascado.
  - Bloqueo de equipar cualquier otra arma mientras la arrojadiza está fuera de la mano (apuntando, lanzada, clavada o regresando), vía `RE::TESEquipEvent`.

### v0.1.3

- Recuperación automática del arma al cambiar de celda (`WeaponManager::OnLoadingScreenClosed`, enganchado al cierre de la pantalla de carga vía `RE::MenuOpenCloseEvent`/`RE::UI`): si el ciclo estaba en marcha (apuntando, lanzada, clavada o regresando) al entrar en un interior, salir de él, viajar rápido, etc., el arma se reequipa sola en vez de obligar a pulsar el botón de recuperar. Probado en el juego en ambas direcciones (interior↔exterior).
  - Se descartaron dos alternativas antes de llegar a esta: `TESCellAttachDetachEvent` filtrado al jugador nunca se dispara para su referencia (igual que en Papyrus, `OnCellAttach`/`OnCellDetach` tampoco lo hacen); `TESCellFullyLoadedEvent` solo salta cuando el motor tiene que cargar datos nuevos, así que no se disparaba al volver a una celda exterior ya visitada/en caché.
  - `EquipObject`/`UnequipObject` fuerzan aplicación inmediata sin cola; además, el reequipar se difiere un tick (tarea de SKSE) en vez de llamarse dentro del propio evento de cierre de carga — invocado en ese instante exacto, el juego aceptaba la orden (sonaba el sonido de equipar) pero nunca llegaba a equipar el arma de verdad.
  - No se toca nada al cargar una partida guardada (no hay forma fiable de saber si el arma sin equipar es por nuestro ciclo o por decisión del jugador tras reiniciar el proceso), se deja tal cual está en el guardado.

### v0.2.1

- Lanzamiento del proyectil réplica (`Throw::LaunchWeapon`, llamado desde `WeaponManager::ThrowWeapon` justo antes de desequipar el arma original):
  - Usa el sistema nativo `RE::Projectile` de Skyrim para dar gratis la trayectoria parabólica (punto 3 de la mecánica) y el clavado en superficie/enemigo (punto 6).
  - Origen del lanzamiento: el nodo `WEAPON` de la mano derecha (misma posición que el arma, punto 2), no la cámara.
  - Dirección: hacia donde apunta la cámara (decisión de diseño, no especificada por el documento), calculada a partir del vector de dirección de la cámara (no de `ToEulerAnglesXYZ`, que descompone en un orden distinto al que usa Skyrim y da ángulos incorrectos en cuanto se mezcla inclinación vertical con giro horizontal).
  - Decisión de arquitectura para el regreso (no implementado todavía, documentada en `CLAUDE.md`): la física nativa vale para la ida pero no para la vuelta (curva no balística, velocidad recalculada, homing a enemigo); el regreso se controlará a mano, sin Havok.

## 2026-07-13

### v0.2.2

- Lanzamiento del proyectil, de verdad funcional de punta a punta (probado en el juego contra paredes y NPCs):
  - Cambiado de tipo `Lobber`/`Missile` a **Arrow**, lanzado vía `RE::Projectile::LaunchArrow` con un formulario `Ammo` dedicado (`CAP_ThorMjolnir_Ammo`) en vez de construir el `LaunchData` a mano — es la misma ruta que usa el motor para flechas reales, y resultó ser necesaria para que el impacto contra actores se registre (daño) y para que el arma quede clavada en vez de destruirse al chocar.
  - `Lobber`/`Grenade` no servía: usa un modelo de trayectoria distinto (pensado para lanzar hacia un punto de impacto calculado) que no respondía al par dirección+velocidad que le pasábamos, dejando el proyectil flotando sin moverse.
  - La capa de colisión (`Collision Layer`) del formulario `Projectile` estaba vacía en la Creation Kit; sin ella asignada (a `L_PROJECTILE`, la misma que usan las flechas) el proyectil atravesaba paredes y actores sin registrar ningún impacto.
  - Desmarcado "Can be Picked Up" en el `Projectile`: al quedar clavado, se podía recoger como si fuera munición suelta (añadiendo el `Ammo` falso al inventario, no el arma real) dejando además un modelo fantasma en el mundo. La única forma prevista de recuperar el arma es el botón, no recogerla del suelo.

### v0.2.3

- Detección de impacto y distancia máxima (`Throw::TrackProjectile`, en `ThrowProjectile.cpp`), completando lo que quedaba pendiente de `THROW`, probado en el juego contra pared/árbol, NPC y lanzamiento al vacío:
  - `Throw::LaunchWeapon` ahora devuelve el `ProjectileHandle` del proyectil lanzado (antes se descartaba); `WeaponManager::ThrowWeapon` lo persiste en `WeaponState` y arranca su seguimiento.
  - Sondeo del proyectil cada ~50ms: solo `ImpactResult::kImpale`/`kStick` cuentan como "clavada" (punto 6) — `kBounce` (rebote contra una superficie sin quedar clavado, p. ej. una pared en ángulo) no debe darse por terminado, sigue en vuelo.
  - El sondeo **no** se reprograma llamando a `SKSE::GetTaskInterface()->AddTask` directamente desde dentro de sí mismo: si esa cola no está separada por fotogramas, encadenar comprobaciones así congela el juego por completo (comprobado). En su lugar, cada vez que toca seguir vigilando se lanza un hilo aparte que duerme un intervalo de tiempo real y solo entonces reprograma la comprobación en el hilo principal.
  - El golpe contra un actor no se refleja en `ImpactResult` (queda clavado visualmente pero el campo no cambia); se detecta aparte con `RE::TESHitEvent` (`Events::ProjectileHitWatcher`), filtrado por `a_event->source` (el arma causante) — `a_event->projectile` llega a 0 al lanzar vía `LaunchArrow` con un arma como `a_weap` en vez de un arco real, así que no sirve para filtrar.
  - Distancia máxima de lanzamiento (`Constants::kMaxThrowDistance`, 6000 unidades — valor no especificado por el documento, elegido como placeholder), si se supera sin impactar, dispara la recuperación automática (punto 5).
  - El sondeo se autocancela si el estado deja de ser "lanzada" por cualquier otro motivo (botón de recuperar, resincronización tras pantalla de carga) o si el proyectil deja de existir.

### v0.2.4

- Dos huecos detectados al repasar el sondeo de `THROW` antes de abordar `RETURN`:
  - `WeaponManager::RecallWeapon` ahora destruye (`Projectile::Kill`) la réplica en vuelo o clavada al recuperar el arma — proceso inverso al lanzamiento (punto 2). Sin esto, tanto el botón de recuperar como el auto-regreso (distancia máxima, pantalla de carga) dejaban el proyectil abandonado en el mundo para siempre como un arma fantasma.
  - Caída al agua (caso no cubierto por el documento): `ImpactResult` no cambia nunca al caer al agua, así que `Throw::TrackProjectile` no lo detectaba como impacto ni como distancia máxima, y el arma se quedaba flotando/hundiéndose indefinidamente. Se detecta aparte con `TESObjectREFR::IsInWater()` (comparación barata de altura de agua vs. posición Z, apta para el sondeo) y se trata igual que "no impactó": recuperación automática (`WeaponManager::OnProjectileEnteredWater`).
