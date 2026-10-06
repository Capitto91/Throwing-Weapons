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

-- Comprobador de estructura, a mano con `xmake check-structure` (no va en la compilación): busca en src/
-- código que ya tiene pieza compartida (ver "Piezas compartidas" en CLAUDE.md). Sale con error si encuentra algo.
task("check-structure")
    set_category("plugin")
    on_run(function ()
        -- find: textos literales (o patrones de Lua con pattern = true); allow: archivos donde son legítimos.
        local rules = {
            { find = { "LookupForm", "LookupByEditorID" }, allow = { "src/1.- CORE/Forms.cpp" },
              hint = "formulario del .esp o de Skyrim.esm: puntero en Forms (Forms.h + Forms::Load)" },
            { find = { "HasKeywordString" },
              hint = "keyword en Forms + HasKeyword (el arma: ActorUtils::IsThrowableWeapon)" },
            { find = { "\"WEAPON\"" }, allow = { "src/1.- CORE/Constants.h" },
              hint = "Constants::kWeaponNodeName / ActorUtils::GetWeaponBone" },
            { find = { "GraphVariable%a*%(%s*\"", "NotifyAnimationGraph%(%s*\"" }, pattern = true,
              hint = "nombre del grafo: constante en Constants.h, con las otras del grafo" },
            { find = { "std::thread" }, allow = { "src/1.- CORE/Scheduler.cpp", "src/6.- PHYSICS/PhysicsManager.cpp" },
              hint = "Scheduler::After (un disparo) o Physics::StartTickLoop (cada fotograma)" },
            { find = { "SetDelete" }, allow = { "src/6.- PHYSICS/PhysicsManager.cpp" },
              hint = "Physics::DestroyReference" },
            { find = { "EquipObject(", "UnequipObject(" }, allow = { "src/11.- SKYRIM/ActorUtils.cpp", "src/10.- EVENTS/EventManager.cpp" },
              hint = "ActorUtils::EquipNow / UnequipNow" },
            { find = { "_mm_store_ps" }, allow = { "src/9.- MATH/VectorMath.cpp" },
              hint = "Math::ToNiPoint3" },
            { find = { "/ 180", "/180" }, allow = { "src/9.- MATH/RotationMath.h" },
              hint = "Math::DegreesToRadians" },
            { find = { "AddAnimationGraphEventSink" }, allow = { "src/10.- EVENTS/AttackInterruptWatcher.cpp" },
              hint = "ActorUtils::AddEventSinkToAllGraphs" },
            { find = { "SKSE::log::" },
              hint = "logs:: (alias de pch.h)" },
            { find = { "0[xX]14%x%x%x%x%x%x%x" }, pattern = true,
              hint = "REL::RelocationID / REL::Relocation con Address Library, nunca direcciones fijas" },
        }

        local function is_allowed(rule, file)
            for _, allowed in ipairs(rule.allow or {}) do
                if file == allowed then
                    return true
                end
            end
            return false
        end

        -- Quita el comentario // del final de la línea, si no está dentro de una cadena.
        local function strip_comment(line)
            local from = 1
            while true do
                local start = line:find("//", from, true)
                if not start then
                    return line
                end
                local _, quotes = line:sub(1, start - 1):gsub("\"", "")
                if quotes % 2 == 0 then
                    return line:sub(1, start - 1)
                end
                from = start + 2
            end
        end

        local root = os.projectdir()
        local files = table.join(os.files(path.join(root, "src/**.cpp")), os.files(path.join(root, "src/**.h")))
        table.sort(files)

        local count = 0
        local checked = 0
        for _, file in ipairs(files) do
            local relative = (path.relative(file, root):gsub("\\", "/"))
            if not relative:find("13.- EXTERNAL", 1, true) then
                checked = checked + 1
                local lineNumber = 0
                for line in (io.readfile(file) .. "\n"):gmatch("(.-)\r?\n") do
                    lineNumber = lineNumber + 1
                    local code = strip_comment(line)
                    for _, rule in ipairs(rules) do
                        if not is_allowed(rule, relative) then
                            for _, needle in ipairs(rule.find) do
                                if code:find(needle, 1, not rule.pattern) then
                                    count = count + 1
                                    print("%s:%d: usar %s", relative, lineNumber, rule.hint)
                                    print("    %s", line:trim())
                                    break
                                end
                            end
                        end
                    end
                end
            end
        end

        if count > 0 then
            raise("check-structure: %d aviso(s).", count)
        end
        print("check-structure: sin avisos (%d archivos).", checked)
    end)
    set_menu {
        usage = "xmake check-structure",
        description = "Busca en src/ lo que ya tiene pieza compartida (ver CLAUDE.md).",
        options = {}
    }
task_end()
