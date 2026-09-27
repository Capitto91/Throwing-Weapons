## Créditos

Este plugin usa o se integra con los siguientes mods. Todo el mérito de su trabajo es de sus autores.

- **[True Directional Movement](https://github.com/ersh1/TrueDirectionalMovement)**, de ersh1 (GPL-3.0). Integración opcional con su target lock. Se incluye, sin modificar, el header de su API (`src/13.- EXTERNAL/TrueDirectionalMovement/`). En `src/11.- SKYRIM/TDMBridge.cpp` se ha copiado su función de puntería predictiva (`PredictAimProjectile`) y reproducido su forma de elegir el punto del cuerpo del objetivo, para que el arma apunte con lock igual que TDM apunta sus flechas.
- **[Open Animation Replacer](https://github.com/ersh1/OpenAnimationReplacer)**, de ersh1 (GPL-3.0). Se incluyen, sin modificar, los archivos de su API de funciones (`src/13.- EXTERNAL/OpenAnimationReplacer/`), usados para sincronizar el plugin con las animaciones de lanzar, llamar y atrapar el arma.
- **[CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng)** (rama `ng` de CommonLibVR, de alandtse) y **[SKSE](https://skse.silverlock.org/)**, base del plugin.
