-- include subprojects
includes("lib/commonlibsse-ng")

-- set project constants
set_project("Throwing-Weapons")
set_version("0.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

-- add common rules
add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

-- third-party dependencies
add_requires("simpleini")

-- define targets
target("Throwing-Weapons")
    add_rules("commonlibsse-ng.plugin", {
        name = "Throwing Weapons",
        author = "Capitto91",
        description = "SKSE64 plugin template using CommonLibSSE-NG"
    })

    -- add src files
    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/pch.h")
    add_packages("simpleini")
