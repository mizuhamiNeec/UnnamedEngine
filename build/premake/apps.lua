function AppLaunchFiles()
	return RootPathList({
		"src/pch.h",
		"src/pch.cpp",
		"src/app/LaunchDesc.h",
		"src/app/GameModuleRegistry.cpp",
		"src/app/GameRuntimeModuleRegistration.h",
		"src/app/GameRuntimeModuleRegistration.cpp",
		"src/app/LoadedGameModule.h",
		"src/app/LoadedGameModule.cpp",
		"src/app/main.cpp",
	})
end

local parkourRuntimeRoot = RootPath("projects/ParkourGame/runtime")
local hasParkourRuntime = os.isdir(parkourRuntimeRoot)
local startupManifest = _G.STARTUP_PROJECT_MANIFEST or "projects/ParkourGame/config/game_profile.json"
local hasStartupManifest = os.isfile(RootPath(startupManifest))

if hasParkourRuntime then
	project "ParkourGameRuntime"
		kind "StaticLib"
		CommonProjectSettings("%{prj.name}")
		WarningSettings()

		files(RootPathList({
			"src/pch.h",
			"src/pch.cpp",
			"projects/ParkourGame/runtime/**.h",
			"projects/ParkourGame/runtime/**.cpp",
		}))

		excludes(RootPathList({
			"projects/ParkourGame/runtime/**/editor/**",
		}))

		EngineIncludeDirs()
		includedirs { parkourRuntimeRoot }
		defines { "UNNAMED_WITH_PARKOUR_RUNTIME" }
		filter {}
end

group "Engine/Applications"

project "UnnamedLauncher"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files(AppLaunchFiles())
	if hasStartupManifest then
		debugargs { "--project=" .. startupManifest }
	end

	EngineIncludeDirs()
	if hasParkourRuntime then
		includedirs { parkourRuntimeRoot }
		defines { "UNNAMED_WITH_PARKOUR_RUNTIME" }
		links { "ParkourGameRuntime" }
	end
	links {
		"UnnamedEngineRuntime",
		"DirectXTex",
	}
	LinkAssimpByConfig()
	CopyDxCompilerDlls()

project "UnnamedEditorApp"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files(AppLaunchFiles())
	if hasStartupManifest then
		debugargs { "--project=" .. startupManifest }
	end

	EngineIncludeDirs()
	links {
		"UnnamedEngineRuntimeEditor",
		"UnnamedEditorRuntime",
		"DirectXTex",
	}
	if hasParkourRuntime then
		includedirs { parkourRuntimeRoot }
		defines { "UNNAMED_WITH_PARKOUR_RUNTIME" }
		links { "ParkourGameRuntime" }
	end
	defines { "UNNAMED_WITH_EDITOR" }
	LinkAssimpByConfig()
	CopyDxCompilerDlls()
