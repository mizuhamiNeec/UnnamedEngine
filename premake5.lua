ENGINE_NAME = "Unnamed"

ARCHITECTURE = "x64"

ROOT_DIR = path.getabsolute(".")
BIN_DIR = path.join(ROOT_DIR, "bin")
INT_DIR = path.join(BIN_DIR, "intermediate")
BUILD_DIR = path.join(ROOT_DIR, "build")
PROJECT_FILES_DIR = path.join(BUILD_DIR, "projects")

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

newoption {
	trigger = "games",
	value = "LIST",
	description = "Build game runtimes/apps (comma-separated: parkour,teamgame,all,none).",
}
newoption {
	trigger = "projects-root",
	value = "PATH",
	description = "Root directory that contains game projects (e.g. S:/Repositories/TD4_01/projects).",
}

function NormalizeGameToken(token)
	if token == nil then
		return ""
	end
	local normalized = string.lower(token)
	normalized = normalized:gsub("%s+", "")
	return normalized
end

function ShouldEnableGame(gameToken, runtimeDir)
	local optionValue = _OPTIONS["games"]
	local hasRuntime = os.isdir(runtimeDir)
	if optionValue == nil or optionValue == "" then
		return false
	end

	local gameRequested = false
	for token in string.gmatch(optionValue, "([^,]+)") do
		local normalized = NormalizeGameToken(token)
		if normalized == "all" then
			return hasRuntime
		end
		if normalized == "none" then
			return false
		end
		if normalized == gameToken then
			gameRequested = true
		end
	end

	if not gameRequested then
		return false
	end
	return hasRuntime
end

PROJECTS_ROOT = _OPTIONS["projects-root"] or os.getenv("UNNAMED_GAME_PROJECTS_ROOT") or "projects"
PARKOUR_RUNTIME_DIR = path.join(PROJECTS_ROOT, "ParkourGame/runtime")
TEAMGAME_RUNTIME_DIR = path.join(PROJECTS_ROOT, "TeamGame/runtime")
ENABLE_PARKOUR_RUNTIME = ShouldEnableGame("parkour", PARKOUR_RUNTIME_DIR)
ENABLE_TEAMGAME_RUNTIME = ShouldEnableGame("teamgame", TEAMGAME_RUNTIME_DIR)

function UnnamedSettings()
	defines {
		"ENGINE_NAME=\"" .. ENGINE_NAME .. "\"",
		"ENGINE_VERSION=\"3.4.0\"",
		"_CRTDBG_MAP_ALLOC",
	}
end

function CommonSettings()
	language "C++"
	cppdialect "C++23"
	architecture(ARCHITECTURE)
	multiprocessorcompile "On"

	debugdir(ROOT_DIR)
	characterset "Unicode"
	filter "system:windows"
		buildoptions { "/utf-8" }
	filter {}
end

function WarningSettings()
	filter "system:windows"
		warnings "Extra"
		buildoptions { "/W4" }
		fatalwarnings { "All" }
		linkoptions { "/IGNORE:4099" } -- Assimpが発狂するので無視
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
		runtime "Release"
		staticruntime "On"
		optimize "Speed"
		defines { "NDEBUG" }
	filter {}
end

function WindowsPlatformSettings()
	filter "system:windows"
		systemversion "latest"
		defines { "NOMINMAX" }
	filter {}
end

function CommonProjectSettings(projectName)
	CommonSettings()
	ConfigurationSettings()
	UnnamedSettings()
	WindowsPlatformSettings()

	location(path.join(PROJECT_FILES_DIR, projectName))
	targetdir(path.join(BIN_DIR, outputdir, projectName))
	objdir(path.join(INT_DIR, outputdir, projectName))
end

function EngineIncludeDirs()
	includedirs {
		"src/",
		"src/thirdparty/assimp/include",
		"src/thirdparty/DirectXTex",
		"src/thirdparty/ImGui",
		"src/thirdparty/ImGuizmo",
		"src/thirdparty/nlohmann",
	}
end

function LinkAssimpByConfig()
	filter "configurations:Debug"
		links { "src/thirdparty/assimp/lib/Debug/assimp-vc143-mdd.lib" }

	filter "configurations:Develop"
		links { "src/thirdparty/assimp/lib/Release/assimp-vc143-md.lib" }

	filter "configurations:Release"
		links { "src/thirdparty/assimp/lib/Release/assimp-vc143-mt.lib" }
	filter {}
end

function CopyDxCompilerDlls()
	postbuildcommands {
		'copy /Y "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}\\dxcompiler.dll"',
		'copy /Y "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}\\dxil.dll"'
	}
end

workspace(ENGINE_NAME)
	configurations { "Debug", "Develop", "Release" }

group "Thirdparty"
project "DirectXTex"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/thirdparty/DirectXTex/*.h",
		"src/thirdparty/DirectXTex/*.cpp",
		"src/thirdparty/DirectXTex/**.h",
		"src/thirdparty/DirectXTex/**.cpp",
	}

	includedirs {
		"src/thirdparty/DirectXTex",
		"src/thirdparty/DirectXTex/Shaders/Compiled",
	}

project "Lua"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files {
		"src/thirdparty/lua/*.h",
		"src/thirdparty/lua/*.c",
	}

	includedirs {
		"src/thirdparty/lua",
	}

group "Engine/Runtime"
project "UnnamedEngineRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/core/**.h",
		"src/core/**.cpp",
		"src/engine/**.h",
		"src/engine/**.cpp",
		"content/**.hlsl",
		"content/**.hlsli",
	}

	filter { "files:content/**.hlsl" }
		excludefrombuild "On"
	filter { "files:content/**.hlsli" }
		excludefrombuild "On"
	filter {}

	excludes {
		"src/thirdparty/**",
		"src/transplantation/**",
		"src/engine/editor/**",
		"src/engine/gui/editor/**",
		"src/engine/ImGui/**",
		"src/engine/ui/ImGuiLayer.h",
		"src/engine/ui/ImGuiLayer.cpp",
		"src/engine/world/EditorWorld.h",
		"src/engine/world/EditorWorld.cpp",
		"src/engine/world/GameWorld.h",
		"src/engine/world/GameWorld.cpp",
		"src/app/**",
	}

	EngineIncludeDirs()
	libdirs { "src/thirdparty/DirectXTex/" }
	links { "DirectXTex", "Lua" }
	LinkAssimpByConfig()

	filter "configurations:Debug"
		defines { "UNNAMED_WITH_EDITOR" }
	filter {}

project "UnnamedEditorRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/engine/editor/**.h",
		"src/engine/editor/**.cpp",
		"src/engine/world/EditorWorld.h",
		"src/engine/world/EditorWorld.cpp",
		"src/engine/ImGui/**.h",
		"src/engine/ImGui/**.cpp",
		"src/engine/ui/ImGuiLayer.h",
		"src/engine/ui/ImGuiLayer.cpp",
		"src/engine/gui/editor/**.h",
		"src/engine/gui/editor/**.cpp",
		"src/engine/unnamed/subsystem/editorluasystem/**.h",
		"src/engine/unnamed/subsystem/editorluasystem/**.cpp",
		"src/thirdparty/ImGui/imgui.cpp",
		"src/thirdparty/ImGui/imgui_draw.cpp",
		"src/thirdparty/ImGui/imgui_widgets.cpp",
		"src/thirdparty/ImGui/imgui_tables.cpp",
		"src/thirdparty/ImGui/imgui_demo.cpp",
		"src/thirdparty/ImGui/imgui_impl_dx12.cpp",
		"src/thirdparty/ImGui/imgui_impl_win32.cpp",
		"src/thirdparty/ImGuizmo/ImGuizmo.cpp",
		"src/thirdparty/ImGuizmo/ImGuizmo.h",
	}

	EngineIncludeDirs()

	filter { "files:src/thirdparty/ImGui/**.cpp or files:src/thirdparty/ImGuizmo/**.cpp" }
		warnings "Extra"
		disablewarnings { "4189" }
	filter {}

	filter "configurations:Debug"
		defines { "UNNAMED_WITH_EDITOR" }
filter {}
	
if ENABLE_PARKOUR_RUNTIME then
	group "Games/Parkour"
	-- Phase 10: Parkour runtime sources moved from src/game to projects/ParkourGame/runtime.
	project "ParkourRuntime"
		kind "StaticLib"
		CommonProjectSettings("%{prj.name}")

		files {
			"src/pch.h",
			"src/pch.cpp",
			PARKOUR_RUNTIME_DIR .. "/**.h",
			PARKOUR_RUNTIME_DIR .. "/**.cpp",
		}

		excludes {
			"src/transplantation/**",
		}

		EngineIncludeDirs()
		includedirs { PARKOUR_RUNTIME_DIR }
end

if ENABLE_TEAMGAME_RUNTIME then
	group "Games/TeamGame"
	-- Phase 11: TeamGame runtime sources moved from src/game to projects/TeamGame/runtime.
	project "TeamGameRuntime"
		kind "StaticLib"
		CommonProjectSettings("%{prj.name}")

		files {
			"src/pch.h",
			"src/pch.cpp",
			TEAMGAME_RUNTIME_DIR .. "/**.h",
			TEAMGAME_RUNTIME_DIR .. "/**.cpp",
		}

		excludes {
			"src/transplantation/**",
		}

		EngineIncludeDirs()
		includedirs { TEAMGAME_RUNTIME_DIR }

	project "TeamGameRuntimeDll"
		kind "SharedLib"
		CommonProjectSettings("%{prj.name}")
		targetname "TeamGameRuntime"

		files {
			"src/pch.h",
			"src/pch.cpp",
			"src/app/runtime/TeamGameRuntimeApiEntry.cpp",
		}

		EngineIncludeDirs()
		includedirs { TEAMGAME_RUNTIME_DIR }
		links {
			"UnnamedEngineRuntime",
			"TeamGameRuntime",
			"DirectXTex",
		}
		filter "configurations:Debug"
			links { "UnnamedEditorRuntime" }
			defines { "UNNAMED_WITH_EDITOR" }
		filter {}
		LinkAssimpByConfig()
		linkoptions { "/WHOLEARCHIVE:TeamGameRuntime.lib" }
end

group "Engine/Applications"
project "UnnamedLauncher"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/app/AppLaunchOptions.h",
		"src/app/GameModuleFactory.h",
		"src/app/GameModuleFactory.cpp",
		"src/app/main.cpp",
	}

	local launcherGameIncludeDirs = {}
	local launcherGameLinks = {}
	local launcherWholeArchiveLinkOptions = {}
	local launcherGameDefines = {}
	if ENABLE_PARKOUR_RUNTIME then
		table.insert(launcherGameIncludeDirs, PARKOUR_RUNTIME_DIR)
		table.insert(launcherGameLinks, "ParkourRuntime")
		table.insert(launcherWholeArchiveLinkOptions, "/WHOLEARCHIVE:ParkourRuntime.lib")
		table.insert(launcherGameDefines, "UNNAMED_WITH_PARKOUR_RUNTIME")
	end
	if ENABLE_TEAMGAME_RUNTIME then
		table.insert(launcherGameIncludeDirs, TEAMGAME_RUNTIME_DIR)
		table.insert(launcherGameLinks, "TeamGameRuntime")
		table.insert(launcherWholeArchiveLinkOptions, "/WHOLEARCHIVE:TeamGameRuntime.lib")
		table.insert(launcherGameDefines, "UNNAMED_WITH_TEAMGAME_RUNTIME")
	end

	EngineIncludeDirs()
	includedirs(launcherGameIncludeDirs)
	links {
		"UnnamedEngineRuntime",
		"DirectXTex",
	}
	links(launcherGameLinks)
	defines(launcherGameDefines)
	filter "configurations:Debug"
		links { "UnnamedEditorRuntime" }
		defines { "UNNAMED_WITH_EDITOR" }
	filter {}
	LinkAssimpByConfig()
	CopyDxCompilerDlls()
	linkoptions(launcherWholeArchiveLinkOptions)

project "UnnamedEditorApp"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files {
		"src/pch.h",
		"src/pch.cpp",
		"src/app/AppLaunchOptions.h",
		"src/app/GameModuleFactory.h",
		"src/app/GameModuleFactory.cpp",
		"src/app/EditorMain.cpp",
	}

	local editorGameIncludeDirs = {}
	local editorGameLinks = {}
	local editorWholeArchiveLinkOptions = {}
	local editorGameDefines = {}
	if ENABLE_PARKOUR_RUNTIME then
		table.insert(editorGameIncludeDirs, PARKOUR_RUNTIME_DIR)
		table.insert(editorGameLinks, "ParkourRuntime")
		table.insert(editorWholeArchiveLinkOptions, "/WHOLEARCHIVE:ParkourRuntime.lib")
		table.insert(editorGameDefines, "UNNAMED_WITH_PARKOUR_RUNTIME")
	end
	if ENABLE_TEAMGAME_RUNTIME then
		table.insert(editorGameIncludeDirs, TEAMGAME_RUNTIME_DIR)
		table.insert(editorGameLinks, "TeamGameRuntime")
		table.insert(editorWholeArchiveLinkOptions, "/WHOLEARCHIVE:TeamGameRuntime.lib")
		table.insert(editorGameDefines, "UNNAMED_WITH_TEAMGAME_RUNTIME")
	end

	EngineIncludeDirs()
	includedirs(editorGameIncludeDirs)
	links {
		"UnnamedEngineRuntime",
		"UnnamedEditorRuntime",
		"DirectXTex",
	}
	links(editorGameLinks)
	defines(editorGameDefines)
	filter "configurations:Debug"
		defines { "UNNAMED_WITH_EDITOR" }
	filter {}
	LinkAssimpByConfig()
	CopyDxCompilerDlls()
	linkoptions(editorWholeArchiveLinkOptions)

if ENABLE_PARKOUR_RUNTIME then
	group "Games/Parkour"
	project "ParkourGameApp"
		kind "WindowedApp"
		CommonProjectSettings("%{prj.name}")

		files {
			"src/pch.h",
			"src/pch.cpp",
			"src/app/AppLaunchOptions.h",
			"src/app/GameModuleFactory.h",
			"src/app/GameModuleFactory.cpp",
			"src/app/GameMain.cpp",
		}

		EngineIncludeDirs()
		includedirs { PARKOUR_RUNTIME_DIR }
		links {
			"UnnamedEngineRuntime",
			"ParkourRuntime",
			"DirectXTex",
		}
		defines { "UNNAMED_WITH_PARKOUR_RUNTIME" }
		filter "configurations:Debug"
			links { "UnnamedEditorRuntime" }
			defines { "UNNAMED_WITH_EDITOR" }
		filter {}
		LinkAssimpByConfig()
		CopyDxCompilerDlls()
		linkoptions { "/WHOLEARCHIVE:ParkourRuntime.lib" }
end

if ENABLE_TEAMGAME_RUNTIME then
	group "Games/TeamGame"
	project "TeamGameApp"
		kind "WindowedApp"
		CommonProjectSettings("%{prj.name}")

		files {
			"src/pch.h",
			"src/pch.cpp",
			"src/app/AppLaunchOptions.h",
			"src/app/GameModuleFactory.h",
			"src/app/GameModuleFactory.cpp",
			"src/app/TeamGameMain.cpp",
		}

		EngineIncludeDirs()
		includedirs { TEAMGAME_RUNTIME_DIR }
		links {
			"UnnamedEngineRuntime",
			"TeamGameRuntime",
			"DirectXTex",
		}
		defines { "UNNAMED_WITH_TEAMGAME_RUNTIME" }
		filter "configurations:Debug"
			links { "UnnamedEditorRuntime" }
			defines { "UNNAMED_WITH_EDITOR" }
		filter {}
		LinkAssimpByConfig()
		CopyDxCompilerDlls()
end
