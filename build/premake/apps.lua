function AppLaunchFiles()
	return RootPathList({
		"src/pch.h",
		"src/pch.cpp",
		"src/app/LaunchDesc.h",
		"src/app/GameProfileLoader.h",
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

function ParkourRuntimeProject(projectName, enableEditor)
	project(projectName)
		kind "StaticLib"
		if enableEditor then
			removeconfigurations { "Develop", "Release" }
		end
		CommonProjectSettings("%{prj.name}")
		PCHSettings()
		WarningSettings()

		files(RootPathList({
			"src/pch.h",
			"src/pch.cpp",
			"projects/ParkourGame/runtime/**.h",
			"projects/ParkourGame/runtime/**.cpp",
		}))

		if not enableEditor then
			excludes(RootPathList({
				"projects/ParkourGame/runtime/**/editor/**",
			}))
		end

		EngineIncludeDirs()
		includedirs { parkourRuntimeRoot }
		defines { "UNNAMED_WITH_PARKOUR_RUNTIME" }
		if enableEditor then
			defines { "UNNAMED_WITH_EDITOR" }
		end
		filter {}
end

if hasParkourRuntime then
	group "Game/Parkour"
	ParkourRuntimeProject("ParkourGameRuntime", false)
	ParkourRuntimeProject("ParkourGameRuntimeEditor", true)
end

group "Engine/Applications"

project "UnnamedLauncher"
	kind "WindowedApp"
	CommonProjectSettings("%{prj.name}")
	PCHSettings()
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

group "Editor/Applications"

project "UnnamedEditorApp"
	kind "WindowedApp"
	removeconfigurations { "Develop", "Release" }
	CommonProjectSettings("%{prj.name}")
	PCHSettings()
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
		links { "ParkourGameRuntimeEditor" }
	end
	defines { "UNNAMED_WITH_EDITOR" }
	LinkAssimpByConfig()
	CopyDxCompilerDlls()
