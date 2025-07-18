ENGINE_NAME = "Unnamed"

ARCHITECTURE = "x64"

ROOT_DIR = path.getabsolute(".")
BIN_DIR = path.join(ROOT_DIR, "bin")
INT_DIR = path.join(BIN_DIR, "intermediate")

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

function CommonSettings()
    language "C++"
    cppdialect "C++23"
    architecture(ARCHITECTURE)
    debugdir (ROOT_DIR)
    characterset "Unicode" -- 文字セットをUnicodeに指定
    filter "system:windows"
        buildoptions { "/utf-8" } -- UTF-8でソースを扱う
    filter {}
end

function WarningSettings()
    filter "system:windows"
        warnings "Extra"
        -- 警告をエラーとして扱う
        buildoptions { "/W4", "/WX" }
    filter {}
end

function ConfigurationSettings()
    filter "configurations:Debug"
        defines { "_DEBUG" }
        symbols "On"
        runtime "Debug"

    filter "configurations:Develop"
        defines { "DEVELOP" }
        symbols "On"
        runtime "Release"

    filter "configurations:Release"
        defines { "NDEBUG" }
        optimize "On"
        runtime "Release"
    filter {}
end

function WindowsPlatformSettings()
    filter "system:windows"
        systemversion "latest"
        defines { "NOMINMAX" }
        defines { "WIN32_LEAN_AND_MEAN" }
    filter {}
end

function PCHSettings()
	pchheader "pch.h"
	pchsource "src/public/pch.cpp"
end

workspace(ENGINE_NAME)
    configurations { "Debug", "Develop", "Release" }

group "Engine"
    project(ENGINE_NAME)
        kind "WindowedApp"
        CommonSettings()
        WarningSettings()
        ConfigurationSettings()

        targetdir(path.join(BIN_DIR, outputdir, "%{prj.name}"))
        objdir(path.join(INT_DIR, outputdir, "%{prj.name}"))

        files {
            "src/public/**.h",
            "src/public/**.cpp",
            "src/private/**.h",
            "src/private/**.cpp",
            "src/shared/**.h",
            "src/shared/**.cpp",
        }

        includedirs {
            "src/public",
            "src/shared",
        }
	
		filter {}