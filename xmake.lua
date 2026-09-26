-- include subprojects
includes("lib/commonlibsse-ng")

-- set project constants
set_project("ThorMjolnir")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- third-party dependencies
add_requires("simpleini")

-- define targets
--
-- Nombre del target (no el del proyecto, fijado arriba con set_project) --
-- la regla commonlibsse-ng.plugin calcula installdir como
-- XSE_TES5_MODS_PATH/<nombre del target> (ver
-- lib/commonlibsse-ng/xmake.lua). "ThorMjolnir_OAR" (exclusivo de esta rama,
-- no de "behavior"/"main") para que este build caiga en su propia carpeta de
-- mod en vez de compartir "ThorMjolnir" con la rama "behavior" -- ambas
-- ramas compilaban al mismo installdir y se pisaban el DLL/INI la una a la
-- otra en el mismo mod manager. Los assets ya existentes bajo la carpeta de
-- mod "ThorMjolnir" (nif/sonidos/ESP/animaciones OAR, gestionados fuera de
-- este repo) siguen sin duplicarse aquí -- pendiente de decidir si hace
-- falta copiarlos a "ThorMjolnir_OAR" para que el mod funcione de forma
-- aislada. Probado un after_config(...) propio para sobrescribir installdir
-- sin renombrar el target -- descartado: xmake solo invoca after_config de
-- *reglas* (config_target en modules/private/utils/target.lua), no del
-- target en sí, así que nunca llegaba a ejecutarse.
--
-- 2026-09-26: el mod publicado ("ThrowableMjolnir" en MO2) es ahora el que
-- carga el DLL -- tiene prioridad sobre "ThorMjolnir_OAR" y tapaba cada
-- build nuevo. Esta regla (sí se ejecuta, a diferencia del after_config de
-- target) se añade después de commonlibsse-ng.plugin para sobrescribir
-- solo la carpeta de instalación, sin renombrar el target: el nombre del
-- DLL ("ThorMjolnir_OAR") es el que piden los config.json de OAR en
-- "requiredPlugin".
rule("thormjolnir.installdir")
    on_config(function(target)
        if os.getenv("XSE_TES5_MODS_PATH") then
            target:set("installdir", path.join(os.getenv("XSE_TES5_MODS_PATH"), "ThrowableMjolnir"))
        end
    end)
rule_end()

target("ThorMjolnir_OAR")
    add_rules("commonlibsse-ng.plugin", {
        name = "ThorMjolnir_OAR",
        author = "Capitto91",
        description = "Arma arrojadiza y retornable (estilo Leviathan Axe) para Skyrim SE/AE -- variante OAR"
    })
    add_rules("thormjolnir.installdir")

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_packages("simpleini")

    -- despliega el INI por defecto junto al DLL (mismo prefixdir que usa
    -- commonlibsse-ng.plugin para el binario)
    add_installfiles("Data/SKSE/Plugins/ThorMjolnir.ini", { prefixdir = "SKSE/Plugins" })
