ENGINE_NAME = "Unnamed"

ARCHITECTURE = "x64"

local rootDirOverride = _G.UNNAMED_ROOT_DIR_OVERRIDE
if rootDirOverride ~= nil and rootDirOverride ~= "" then
	ROOT_DIR = path.getabsolute(rootDirOverride)
else
	ROOT_DIR = path.getabsolute(path.getdirectory(_MAIN_SCRIPT))
end
BIN_DIR = path.join(ROOT_DIR, "bin")
INT_DIR = path.join(BIN_DIR, "intermediate")
BUILD_DIR = path.join(ROOT_DIR, "build")
PROJECT_FILES_DIR = path.join(BUILD_DIR, "projects")

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

function RootPath(p)
	if path.isabsolute(p) then
		return p
	end
	return path.join(ROOT_DIR, p)
end

function RootPathList(paths)
	local rooted = {}
	for _, p in ipairs(paths) do
		table.insert(rooted, RootPath(p))
	end
	return rooted
end

function UnnamedSettings()
	defines {
		"ENGINE_NAME=\"" .. ENGINE_NAME .. "\"",
		"ENGINE_VERSION=\"3.5.0\"",
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
	includedirs(RootPathList({
		"src/",
		"src/thirdparty/assimp/include",
		"src/thirdparty/DirectXTex",
		"src/thirdparty/ImGui",
		"src/thirdparty/ImGuizmo",
		"src/thirdparty/nlohmann",
	}))
end

function LinkAssimpByConfig()
	filter "configurations:Debug"
		links { RootPath("src/thirdparty/assimp/lib/Debug/assimp-vc143-mdd.lib") }

	filter "configurations:Develop"
		links { RootPath("src/thirdparty/assimp/lib/Release/assimp-vc143-md.lib") }

	filter "configurations:Release"
		links { RootPath("src/thirdparty/assimp/lib/Release/assimp-vc143-mt.lib") }
	filter {}
end

function CopyDxCompilerDlls()
	postbuildcommands {
		'copy /Y "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}\\dxcompiler.dll"',
		'copy /Y "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}\\dxil.dll"'
	}
end

function PCHSettings()
	pchheader "pch.h"
	pchsource(RootPath("src/pch.cpp"))

	filter { "files:**.cpp", "files:not **/thirdparty/**" }
		forceincludes { "pch.h" }
	filter {}
end
