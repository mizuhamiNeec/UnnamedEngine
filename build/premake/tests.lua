group "Engine/Tests"

project "UnnamedAssetPathContractTests"
	kind "ConsoleApp"
	CommonProjectSettings("%{prj.name}")
	PCHSettings()
	WarningSettings()

	files(RootPathList({
		"src/pch.h",
		"src/pch.cpp",
		"src/tests/AssetPathContractTests.cpp",
	}))

	EngineIncludeDirs()
	links {
		"UnnamedEngineRuntime",
		"DirectXTex",
	}
	LinkAssimpByConfig()
	postbuildcommands {
		'"%{cfg.buildtarget.abspath}"',
	}
