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
