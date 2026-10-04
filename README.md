## Créditos

Este plugin usa o se integra con los siguientes mods. Todo el mérito de su trabajo es de sus autores.

- **[True Directional Movement](https://github.com/ersh1/TrueDirectionalMovement)**, de ersh1 (GPL-3.0). Integración opcional con su target lock. Se incluye, sin modificar, el header de su API (`src/13.- EXTERNAL/TrueDirectionalMovement/`). En `src/11.- SKYRIM/TDMBridge.cpp` se ha copiado su función de puntería predictiva (`PredictAimProjectile`) y reproducido su forma de elegir el punto del cuerpo del objetivo, para que el arma apunte con lock igual que TDM apunta sus flechas.
- **[Open Animation Replacer](https://github.com/ersh1/OpenAnimationReplacer)**, de ersh1 (GPL-3.0). Se incluyen, sin modificar, los archivos de su API de funciones (`src/13.- EXTERNAL/OpenAnimationReplacer/`), usados para sincronizar el plugin con las animaciones de lanzar, llamar y atrapar el arma.
- **[Precision](https://github.com/ersh1/Precision)**, de ersh1. Se ha adaptado su sistema de estelas de ataque (`AttackTrail`): el reciclado de segmentos de la estela del arma (`src/8.- ANIMATION/WeaponTrail.cpp`) y la forma de crear, mover y retirar efectos con `BSTempEffectParticle`, usada también para las chispas (`src/8.- ANIMATION/WeaponVFX.cpp`). Su código sirvió además para localizar la función nativa de daño (`src/1.- CORE/GameOffsets.h`). La malla de la estela (`ThorMjolnirTrail.nif`) parte de la estela de Precision. La estela y la función de daño se tomaron en julio de 2026, cuando Precision tenía licencia MIT (aviso abajo); desde su versión 2.0.5 es GPL-3.0 con las mismas excepciones que este proyecto.
- **[SKSE Menu Framework](https://github.com/QTR-Modding/SKSE-Menu-Framework-3-API)** (versión 3, QTR-Modding; LGPL-2.1 la API). Menú de configuración opcional en el juego. Se incluye, sin modificar, el header de su API (`src/13.- EXTERNAL/SKSEMenuFramework/`, con su licencia).
- **[CommonLibSSE-NG](https://github.com/alandtse/CommonLibVR/tree/ng)** (rama `ng` de CommonLibVR, de alandtse) y **[SKSE](https://skse.silverlock.org/)**, base del plugin.

### Aviso de licencia de Precision (MIT)

Licencia de Precision anterior a su versión 2.0.5, bajo la que se tomaron la estela y la función de daño:

```
MIT License

Copyright (c) 2023 Ersh

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
