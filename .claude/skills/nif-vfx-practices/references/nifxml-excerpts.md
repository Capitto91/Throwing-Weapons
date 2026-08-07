# Extractos verbatim de nif.xml (UVs en movimiento y partículas)

**Fuente:** `https://raw.githubusercontent.com/niftools/nifxml/develop/nif.xml`
(rama `develop` del repo oficial de NifTools — la misma fuente que usa
NifSkope para sus definiciones de bloque). Descargado y citado el 2026-08-07.

Esto es el **formato del fichero** (qué bloques/campos existen y cómo se
enlazan), no una confirmación de que produzcan el resultado visual esperado
en el juego — eso sigue sin verificar contra un NIF real que lo use. Ver
`SKILL.md` para el nivel de confianza de cada afirmación.

## UVs en movimiento — BSEffectShaderProperty + BSEffectShaderPropertyFloatController

```
<niobject name="BSEffectShaderProperty" inherit="BSShaderProperty" ... versions="#SKY_AND_LATER#">
    ...
    <field name="UV Offset" type="TexCoord">Offset UVs</field>
    <field name="UV Scale" type="TexCoord" default="#VEC2_ONE#">Offset UV Scale to repeat tiling textures</field>
    ...
</niobject>

<niobject name="BSEffectShaderPropertyFloatController" inherit="NiFloatInterpController" module="BSAnimation" versions="#SKY_AND_LATER#">
    This controller is used to animate float variables in BSEffectShaderProperty.
    <field name="Controlled Variable" type="EffectShaderControlledVariable">Which float variable in BSEffectShaderProperty to animate.</field>
</niobject>

<enum name="EffectShaderControlledVariable" storage="uint" prefix="ESCV" versions="#SKY_AND_LATER#">
    An unsigned 32-bit integer, describing which float variable in BSEffectShaderProperty to animate.
    <option value="0" name="EmissiveMultiple">EmissiveMultiple.</option>
    <option value="1" name="Falloff Start Angle">Falloff Start Angle (degrees).</option>
    <option value="2" name="Falloff Stop Angle">Falloff Stop Angle (degrees).</option>
    <option value="3" name="Falloff Start Opacity">Falloff Start Opacity.</option>
    <option value="4" name="Falloff Stop Opacity">Falloff Stop Opacity.</option>
    <option value="5" name="Alpha Transparency">Alpha Transparency (Emissive alpha?).</option>
    <option value="6" name="U Offset">U Offset.</option>
    <option value="7" name="U Scale">U Scale.</option>
    <option value="8" name="V Offset">V Offset.</option>
    <option value="9" name="V Scale">V Scale.</option>
    <option value="11" name="Unknown 11" />
    <option value="12" name="Unknown 12" />
    <option value="13" name="Unknown 13" />
    <option value="14" name="Unknown 14" />
</enum>
```

Cadena de enlace de un controlador (genérica, no específica de UV — así es
como cualquier controlador se engancha a cualquier propiedad/nodo):

```
<niobject name="NiObjectNET" ...>
    <field name="Controller" type="Ref" template="NiTimeController" since="3.0">Controller object index. (The first in a chain)</field>
</niobject>

<niobject name="NiTimeController" abstract="true" inherit="NiObject" ...>
    <field name="Next Controller" type="Ref" template="NiTimeController">Index of the next controller.</field>
    <field name="Target" type="Ptr" template="NiObjectNET" since="3.3.0.13">Controller target (object index of the first controllable ancestor of this object).</field>
    ...
</niobject>

<niobject name="NiSingleInterpController" abstract="true" inherit="NiInterpController" ...>
    <field name="Interpolator" type="Ref" template="NiInterpolator" since="10.1.0.104" />
</niobject>

<niobject name="NiFloatInterpolator" inherit="NiKeyBasedInterpolator" ...>
    <field name="Value" type="float" default="#INV_FLT#">Pose value if lacking NiFloatData.</field>
    <field name="Data" type="Ref" template="NiFloatData" />
</niobject>

<niobject name="NiFloatData" inherit="NiObject" ...>
    <field name="Data" type="KeyGroup" template="float">The keys.</field>
</niobject>
```

**Lectura de esto:** `BSEffectShaderProperty` ya tiene sus propios campos
`UV Offset`/`UV Scale`, pero para *animarlos* hace falta un
`BSEffectShaderPropertyFloatController` colgado de la cadena `Controller`
del propio `BSEffectShaderProperty`, con `Controlled Variable` = `U Offset`
(6) o `V Offset` (8) (o `U Scale`/`V Scale`, 7/9), y un `Interpolator` →
`NiFloatInterpolator` → `Data` → `NiFloatData` con las claves de animación
(p. ej. 0.0 → 1.0 en bucle = textura que se desplaza/"scrollea", técnica
típica de energía/fuego/agua que fluye).

**Alternativa legada (no recomendada sin más evidencia):** el propio
`nif.xml` marca `NiUVController`/`NiUVData` como
`DEPRECATED (pre-10.1), REMOVED (20.3)` — existe en el rango de versión de
Skyrim SE (20.2.0.7) pero es el mecanismo antiguo (Morrowind/Oblivion-era),
no el integrado en el shader que usa Skyrim. No usar salvo que el usuario
lo pida explícitamente o aparezca así en un NIF real de referencia.

## Partículas — NiParticleSystem / BSStripParticleSystem

```
<niobject name="NiParticleSystem" inherit="NiParticles" module="NiParticle">
    <field name="Data" type="Ref" template="NiPSysData" vercond="#BS_GTE_SSE#" />
    <field name="World Space" type="bool" default="true">If true, Particles are birthed into world space...</field>
    <field name="Num Modifiers" type="uint" />
    <field name="Modifiers" type="Ref" template="NiPSysModifier" length="Num Modifiers">The list of particle modifiers.</field>
</niobject>

<niobject name="BSStripParticleSystem" inherit="NiParticleSystem" module="BSParticle" versions="#FO3_AND_LATER#">
    Bethesda-Specific (mesh?) Particle System.
</niobject>
```

`NiParticleSystem` (o `BSStripParticleSystem`, variante Bethesda tipo
"tira/ribbon", pensada para estelas) es un nodo de geometría más — se
engancha al árbol de escena y lleva su propia propiedad de shader igual que
cualquier `BSTriShape` (la guía de la herramienta "VFX Editor" en Nexus, al
describir cómo montar un sistema funcional, dice explícitamente: añadir el
Particle System, conectarlo a Root, **añadir un Effect Shader y conectarlo
al Particle System**, añadir un Emitter conectado al Particle System y a
Root, y poner valores no nulos de Birth Rate/Life Span/Size en el Emitter —
fuente: nexusmods.com/skyrimspecialedition/mods/57247, descripción del mod).

### Orden de los modificadores — es una prioridad numérica, no la posición en la lista

```
<enum name="NiPSysModifierOrder" storage="uint">
    <option value="0" name="ORDER_KILLOLDPARTICLES" />
    <option value="1" name="ORDER_BSLOD" />
    <option value="1000" name="ORDER_EMITTER" />
    <option value="2000" name="ORDER_SPAWN" />
    <option value="2500" name="ORDER_FO3_BSSTRIPUPDATE" />
    <option value="3000" name="ORDER_GENERAL" />
    <option value="4000" name="ORDER_FORCE" />
    <option value="5000" name="ORDER_COLLIDER" />
    <option value="6000" name="ORDER_POS_UPDATE" />
    <option value="6500" name="ORDER_POSTPOS_UPDATE" />
    <option value="6600" name="ORDER_WORLDSHIFT_PARTSPAWN" />
    <option value="7000" name="ORDER_BOUND_UPDATE" />
    <option value="8000" name="ORDER_SK_BSSTRIPUPDATE" />
</enum>
```
Comentario textual del propio enum: *"Note: For Skyrim, BSPSysStripUpdateModifier
is 8000 and for FO3 it is 2500."*

Cada `NiPSysModifier` lleva su propio campo `Order` (con un valor por
defecto distinto según el tipo concreto, ver tabla `<default onlyT="...">`
en `NiPSysModifier` — p. ej. `NiPSysAgeDeathModifier` → `ORDER_KILLOLDPARTICLES`,
`NiPSysGravityModifier` → `ORDER_FORCE`, `NiPSysPositionModifier` →
`ORDER_POS_UPDATE`, `NiPSysBoundUpdateModifier` → `ORDER_BOUND_UPDATE`).

### Bloques individuales relevantes (campos exactos)

```
<niobject name="NiPSysModifier" abstract="true" inherit="NiObject">
    <field name="Name" type="string">Used to locate the modifier.</field>
    <field name="Order" type="NiPSysModifierOrder" default="ORDER_GENERAL" />
    <field name="Target" type="Ptr" template="NiParticleSystem">NiParticleSystem parent of this modifier.</field>
    <field name="Active" type="bool" default="true" />
</niobject>

<niobject name="NiPSysEmitter" abstract="true" inherit="NiPSysModifier">
    <field name="Speed" type="float">Speed / Inertia of particle movement.</field>
    <field name="Speed Variation" type="float" />
    <field name="Declination" type="float" />
    <field name="Declination Variation" type="float" />
    <field name="Planar Angle" type="float" />
    <field name="Planar Angle Variation" type="float" />
    <field name="Initial Color" type="Color4" default="#VEC4_ONE#">Defines color of a birthed particle.</field>
    <field name="Initial Radius" type="float" default="1.0">Size of a birthed particle.</field>
    <field name="Radius Variation" type="float" since="10.4.0.1" />
    <field name="Life Span" type="float">Duration until a particle dies.</field>
    <field name="Life Span Variation" type="float" />
</niobject>

<niobject name="NiPSysVolumeEmitter" abstract="true" inherit="NiPSysEmitter">
    <field name="Emitter Object" type="Ptr" template="NiNode" since="10.1.0.0" />
</niobject>

<niobject name="NiPSysBoxEmitter" inherit="NiPSysVolumeEmitter">
    <field name="Width" type="float" />
    <field name="Height" type="float" />
    <field name="Depth" type="float" />
</niobject>

<niobject name="NiPSysMeshEmitter" inherit="NiPSysEmitter">
    <field name="Num Emitter Meshes" type="uint" />
    <field name="Emitter Meshes" type="Ptr" template="NiAVObject" length="Num Emitter Meshes" />
    <field name="Initial Velocity Type" type="VelocityType" />
    <field name="Emission Type" type="EmitFrom" />
    <field name="Emission Axis" type="Vector3" default="#X_AXIS#" />
</niobject>

<niobject name="NiPSysAgeDeathModifier" inherit="NiPSysModifier">
    <field name="Spawn on Death" type="bool" />
    <field name="Spawn Modifier" type="Ref" template="NiPSysSpawnModifier" />
</niobject>

<niobject name="NiPSysPositionModifier" inherit="NiPSysModifier">
    Particle modifier that updates the particle positions based on velocity and last update time.
    (sin campos propios)
</niobject>

<niobject name="NiPSysBoundUpdateModifier" inherit="NiPSysModifier">
    <field name="Update Skip" type="ushort" />
</niobject>

<niobject name="NiPSysGravityModifier" inherit="NiPSysModifier">
    <field name="Gravity Object" type="Ptr" template="NiAVObject" />
    <field name="Gravity Axis" type="Vector3" default="#X_AXIS#" />
    <field name="Decay" type="float" />
    <field name="Strength" type="float" default="1.0" />
    <field name="Force Type" type="ForceType" />
    <field name="Turbulence" type="float" />
    <field name="Turbulence Scale" type="float" default="1.0" />
</niobject>

<niobject name="NiPSysColorModifier" inherit="NiPSysModifier">
    <field name="Data" type="Ref" template="NiColorData" />
</niobject>

<niobject name="NiPSysRotationModifier" inherit="NiPSysModifier">
    <field name="Rotation Speed" type="float" />
    <field name="Rotation Speed Variation" type="float" since="20.0.0.2" />
    <field name="Rotation Angle" type="float" since="20.0.0.2" />
    <field name="Rotation Angle Variation" type="float" since="20.0.0.2" />
    <field name="Random Rot Speed Sign" type="bool" since="20.0.0.2" />
    <field name="Random Axis" type="bool" default="true" />
    <field name="Axis" type="Vector3" default="#X_AXIS#" />
</niobject>

<niobject name="NiPSysUpdateCtlr" inherit="NiTimeController">
    Particle system controller, tells the system to update its simulation.
    (sin campos propios — es el "motor" de la simulación, imprescindible)
</niobject>

<niobject name="NiPSysEmitterCtlr" inherit="NiPSysModifierCtlr">
    NiInterpController::GetInterpolatorID() string format:
        ['BirthRate', 'EmitterActive'] (for "Interpolator" and "Visibility Interpolator" respectively)
    <field name="Data" type="Ref" template="NiPSysEmitterCtlrData" until="10.1.0.103" />
    <field name="Visibility Interpolator" type="Ref" template="NiInterpolator" since="10.1.0.104" />
</niobject>

<niobject name="BSPSysStripUpdateModifier" inherit="NiPSysModifier" module="BSParticle" versions="#FO3_AND_LATER#">
    <field name="Update Delta Time" type="float" default="0.033333" />
</niobject>
```

**Dato importante no obvio:** la "Birth Rate" (cuántas partículas nacen por
segundo) **no es un campo directo de `NiPSysEmitter`** — según el propio
comentario de `NiPSysEmitterCtlr`, su interpolador se identifica como
`'BirthRate'`. Es decir, la tasa de nacimiento se controla vía un
`NiPSysEmitterCtlr` (un `NiTimeController` más, con su propio
`Interpolator`), no rellenando un campo numérico simple en el emisor.

**Conjunto mínimo para un sistema de partículas que nazca, se mueva y
muera** (deducido de combinar los campos anteriores — el propio `NiPSysData`
no lo dice explícito en un solo sitio, así que trátalo como lectura de la
estructura, no como una receta ya probada en el juego):
`NiParticleSystem`/`BSStripParticleSystem` con `Data` → `NiPSysData`, y en
`Modifiers`: un emisor (`NiPSysBoxEmitter`/`NiPSysMeshEmitter`, con
`Life Span`/`Initial Radius` no nulos), `NiPSysPositionModifier` (para que
se muevan), `NiPSysAgeDeathModifier` (para que mueran) y
`NiPSysBoundUpdateModifier`; más un `NiPSysUpdateCtlr` y un
`NiPSysEmitterCtlr` (con su interpolador `'BirthRate'`) colgados como
controladores del propio `NiParticleSystem`. Para el estilo "tira/estela"
de Skyrim, añadir `BSPSysStripUpdateModifier` (Order 8000 en Skyrim) sobre
un `BSStripParticleSystem`.

## Confirmación cruzada: NIFs reales que combinan esto en producción

`_reference/Nif examples/meshes/stormcalling*/` (mods de magia de rayos
publicados, "Storm Calling"/"Storm Calling II") — inspeccionados el
2026-08-07 con el método (a) de `SKILL.md` (grep binario de nombres de
tipo de bloque; PyFFI falla en estos ficheros igual que en
`Blade_of_Chaos*.nif`, por `BSTriShape`). Confirma que la combinación
`NiParticleSystem`/`BSStripParticleSystem` + los modifiers/controladores de
arriba **no es solo teoría del formato**, se usa de verdad junto en mods
publicados. El valor exacto de campos concretos (p. ej. `Controlled
Variable` de un `BSEffectShaderPropertyFloatController` en estos ficheros)
no se podía sacar del grep binario en sí — pero **ya se decodificó** con el
método (c) de `SKILL.md` (lector binario dirigido, validado 2026-08-07): en
`stafflightningproj.nif`, 3 de 5 instancias animan `V Offset` y 2 animan
`Alpha Transparency`. Ver la sección "UVs en movimiento" de `SKILL.md` para
el detalle completo y el resto de ficheros decodificados.

## Bloques adicionales vistos en esos NIFs reales, extraídos de `nif.xml` (2026-08-07)

```
<niobject name="BSPSysScaleModifier" inherit="NiPSysModifier" module="BSParticle" versions="#BETHESDA#">
    <field name="Num Scales" type="uint" />
    <field name="Scales" type="float" length="Num Scales" />
</niobject>

<niobject name="BSPSysSimpleColorModifier" inherit="NiPSysModifier" module="BSParticle" versions="#FO3_AND_LATER#">
    Bethesda-specific particle modifier.
    <field name="Fade In Percent" type="float" default="0.1" range="#F0_1#" />
    <field name="Fade Out Percent" type="float" default="0.9" range="#F0_1#" />
    <field name="Color 1 End Percent" type="float" range="#F0_1#" />
    <field name="Color 1 Start Percent" type="float" range="#F0_1#" />
    <field name="Color 2 End Percent" type="float" range="#F0_1#" />
    <field name="Color 2 Start Percent" type="float" default="1.0" range="#F0_1#" />
    <field name="Colors" type="Color4" length="3" />
</niobject>

<niobject name="BSPSysSubTexModifier" inherit="NiPSysModifier" module="BSParticle" versions="#BETHESDA#">
    Similar to a Flip Controller, this handles particle texture animation on a single texture atlas
    <field name="Start Frame" type="float">Starting frame/position on atlas</field>
    <field name="Start Frame Fudge" type="float" default="64.0" />
    <field name="End Frame" type="float" default="63.0">Ending frame/position on atlas</field>
    <field name="Loop Start Frame" type="float">Frame to start looping</field>
    <field name="Loop Start Frame Fudge" type="float" />
    <field name="Frame Count" type="float" default="30.0" />
    <field name="Frame Count Fudge" type="float" />
</niobject>

<niobject name="BSPSysLODModifier" inherit="NiPSysModifier" module="BSParticle" versions="#BETHESDA#">
    <field name="LOD Begin Distance" type="float" default="0.1" />
    <field name="LOD End Distance" type="float" default="0.7" />
    <field name="End Emit Scale" type="float" default="0.2" />
    <field name="End Size" type="float" default="1.0" />
</niobject>

<niobject name="NiPSysEmitterSpeedCtlr" inherit="NiPSysModifierFloatCtlr" module="NiParticle">
    Animates the speed value on an NiPSysEmitter object.
</niobject>

<niobject name="NiPSysBombModifier" inherit="NiPSysModifier" module="NiParticle">
    Particle modifier that applies an explosive force to particles.
    <field name="Bomb Object" type="Ptr" template="NiNode" />
    <field name="Bomb Axis" type="Vector3" />
    <field name="Decay" type="float" />
    <field name="Delta V" type="float" />
    <field name="Decay Type" type="DecayType" />
    <field name="Symmetry Type" type="SymmetryType" />
</niobject>

<niobject name="BSLagBoneController" inherit="NiTimeController" module="BSAnimation" versions="#SKY_AND_LATER#">
    A controller that trails a bone behind an actor.
    <field name="Linear Velocity" type="float" default="3.0" range="0.0:500.0" />
    <field name="Linear Rotation" type="float" default="1.0" range="0.0:15.0" />
    <field name="Maximum Distance" type="float" default="400.0" range="#F0_1000#" />
</niobject>

<niobject name="BSProceduralLightningController" inherit="NiTimeController" module="BSAnimation" versions="#BETHESDA#">
    Skyrim, Paired with dummy TriShapes, this controller generates lightning shapes for special effects.
    First interpolator controls Generation.
    <field name="Interpolator 1: Generation" type="Ref" template="NiInterpolator" />
    <field name="Interpolator 2: Mutation" type="Ref" template="NiInterpolator" />
    <field name="Interpolator 3: Subdivision" type="Ref" template="NiInterpolator" />
    <field name="Interpolator 4: Num Branches" type="Ref" template="NiInterpolator" />
    <field name="Interpolator 5: Num Branches Var" type="Ref" template="NiInterpolator" />
    <field name="Interpolator 6: Length" type="Ref" template="NiInterpolator" />
    <field name="Interpolator 7: Length Var" type="Ref" template="NiInterpolator" />
    <field name="Interpolator 8: Width" type="Ref" template="NiInterpolator" />
    <field name="Interpolator 9: Arc Offset" type="Ref" template="NiInterpolator">0=straight, 50=wide</field>
    <field name="Subdivisions" type="ushort" default="6" range="0:12" />
    <field name="Num Branches" type="ushort" default="1" range="0:10" />
    <field name="Num Branches Variation" type="ushort" default="1" range="0:10" />
    <field name="Length" type="float" default="512.0">How far lightning will stretch to.</field>
    <field name="Length Variation" type="float" default="30.0" />
    <field name="Width" type="float" default="16.0">How wide the bolt will be.</field>
    <field name="Child Width Mult" type="float" default="0.75" />
    <field name="Arc Offset" type="float" default="20.0" />
    <field name="Fade Main Bolt" type="bool" default="true" />
    <field name="Fade Child Bolts" type="bool" default="true" />
    <field name="Animate Arc Offset" type="bool" default="true" />
    <field name="Shader Property" type="Ref" template="BSShaderProperty" />
</niobject>

<niobject name="NiPSysCylinderEmitter" inherit="NiPSysVolumeEmitter" module="NiParticle">
    <field name="Radius" type="float" />
    <field name="Height" type="float" />
</niobject>

<niobject name="NiPSysSpawnModifier" inherit="NiPSysModifier" module="NiParticle">
    <field name="Num Spawn Generations" type="ushort" default="0" />
    <field name="Percentage Spawned" type="float" default="1.0" />
    <field name="Min Num to Spawn" type="ushort" default="1" />
    <field name="Max Num to Spawn" type="ushort" default="1" />
    <field name="Spawn Speed Variation" type="float" />
    <field name="Spawn Dir Variation" type="float" />
    <field name="Life Span" type="float">Lifespan assigned to spawned particles.</field>
    <field name="Life Span Variation" type="float" />
</niobject>

<niobject name="NiPSysDragModifier" inherit="NiPSysModifier" module="NiParticle">
    Particle modifier that applies a linear drag force to particles.
    <field name="Drag Object" type="Ptr" template="NiAVObject" />
    <field name="Drag Axis" type="Vector3" default="#X_AXIS#" />
    <field name="Percentage" type="float" default="0.05" />
    <field name="Range" type="float" default="#FLT_MAX#" />
    <field name="Range Falloff" type="float" default="#FLT_MAX#" />
</niobject>

<niobject name="NiPSysModifierActiveCtlr" inherit="NiPSysModifierBoolCtlr" module="NiParticle">
    A particle system modifier controller that animates active/inactive state for particles.
</niobject>

<niobject name="BSStripPSysData" inherit="NiPSysData" module="BSParticle" versions="#FO3_AND_LATER#">
    Bethesda-Specific (mesh?) Particle System Data.
    <field name="Max Point Count" type="ushort" />
    <field name="Start Cap Size" type="float" />
    <field name="End Cap Size" type="float" />
    <field name="Do Z Prepass" type="bool" />
</niobject>
```

## Orden de slots de BSShaderTextureSet

`BSShaderTextureSet` solo se referencia desde `BSLightingShaderProperty`
(`Texture Set`, campo `Ref`) — no desde `BSEffectShaderProperty`, que usa
campos de texto directos (`Source Texture`, `Greyscale Texture`,
`Env Map Texture`, `Normal Texture`, `Env Mask Texture`), sin
`BSShaderTextureSet` de por medio.

```
<niobject name="BSShaderTextureSet" inherit="NiObject" module="BSMain" versions="#BETHESDA#">
    Bethesda-specific Texture Set.
    <field name="Num Textures" type="uint" default="6" />
    <field name="Textures" type="SizedString" length="Num Textures">Textures.
        0: Diffuse
        1: Normal/Gloss
        2: Glow(SLSF2_Glow_Map)/Skin/Hair/Rim light(SLSF2_Rim_Lighting)
        3: Height/Parallax
        4: Environment
        5: Environment Mask
        6: Subsurface for Multilayer Parallax
        7: Back Lighting Map (SLSF2_Back_Lighting)
    </field>
</niobject>
```

## Estela de arma (weapon trail) — bloques nuevos vistos en Precision y vanilla

**Fuente de los NIFs:** `_reference/Nif examples/meshes/Effects/WeaponTrails/AttackTrail.nif`
y `AttackTrailMagic.nif` (mod Precision, Ershin), y
`_reference/Nif examples/meshes/weapons/orcish/orcisharrowprojectile.nif` /
`meshes/dlc01/weapons/crossbow/boltprojectile.nif` /
`magic/lightspellprojectile.nif` (mallas vanilla de Skyrim). Inspeccionados
el 2026-08-07 con el método (a) de `SKILL.md` (grep binario). Ver la
sección "Estela de arma" de `SKILL.md` para la lectura completa.

`BSLightingShaderPropertyFloatController` (visto en `AttackTrail.nif`,
junto a un nodo llamado `Refract`) es el equivalente, para
`BSLightingShaderProperty`, del `BSEffectShaderPropertyFloatController` ya
documentado arriba para `BSEffectShaderProperty` — mismo mecanismo, otro
shader. Su enum de variable controlada **incluye Refraction Strength y
también U/V Offset/Scale**, con valores de opción distintos a los de
`EffectShaderControlledVariable`:

```
<niobject name="BSLightingShaderPropertyFloatController" inherit="NiFloatInterpController" module="BSAnimation" versions="#SKY_AND_LATER#">
    This controller is used to animate float variables in BSLightingShaderProperty.
    <field name="Controlled Variable" type="LightingShaderControlledFloat">Which float variable in BSLightingShaderProperty to animate.</field>
</niobject>

<enum name="LightingShaderControlledFloat" storage="uint" prefix="LSCF" versions="#SKY_AND_LATER#">
    An unsigned 32-bit integer, describing which float variable in BSLightingShaderProperty to animate.
    <option value="0" name="Refraction Strength">The amount of distortion.</option>
    <option value="3" name="Unknown 3" />
    <option value="4" name="Unknown 4" />
    <option value="8" name="Environment Map Scale">Environment Map Scale.</option>
    <option value="9" name="Glossiness">Glossiness.</option>
    <option value="10" name="Specular Strength">Specular Strength.</option>
    <option value="11" name="Emissive Multiple">Emissive Multiple.</option>
    <option value="12" name="Alpha">Alpha.</option>
    <option value="13" name="Unknown 13" />
    <option value="14" name="Unknown 14" />
    <option value="20" name="U Offset">U Offset.</option>
    <option value="21" name="U Scale">U Scale.</option>
    <option value="22" name="V Offset">V Offset.</option>
    <option value="23" name="V Scale">V Scale.</option>
</enum>
```

**Confirmado** (método (c) de `SKILL.md`, decodificado 2026-08-07, ya no
solo hipótesis por el nombre del nodo): el `Controlled Variable` real de
`AttackTrail.nif` y `AttackTrailMagic.nif` vale `0` = **Refraction
Strength**. El trail base de Precision anima distorsión, no UV ni alpha —
coherente con el nombre del nodo (`Refract`).

Otros dos bloques nuevos, vistos en las mallas vanilla:

```
<niobject name="NiBillboardNode" inherit="NiNode" module="NiMain">
    These nodes will always be rotated to face the camera creating a billboard effect for any attached objects.
    <field name="Billboard Mode" type="BillboardMode" since="10.1.0.0">The way the billboard will react to the camera.</field>
</niobject>

<niobject name="BSValueNode" inherit="NiNode" module="BSMain" versions="#FO3_AND_LATER#">
    Bethesda-specific node. Found on fxFire effects
    <field name="Value" type="uint" />
    <field name="Value Node Flags" type="BSValueNodeFlags" />
</niobject>
```

`NiBillboardNode` envuelve los emisores en `lightspellprojectile.nif`
(sprites que siempre miran a cámara). `BSValueNode` aparece en
`orcisharrowprojectile.nif`/`boltprojectile.nif` — el propio `nif.xml` dice
"Found on fxFire effects", sin más detalle sobre para qué se usa
exactamente aquí; no se ha investigado más a fondo, no asumir su función
sin comprobarlo si hace falta usarlo.
