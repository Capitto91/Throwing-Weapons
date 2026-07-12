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
