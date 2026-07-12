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
