group "Thirdparty"

project "DirectXTex"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files(RootPathList({
		"src/thirdparty/DirectXTex/*.h",
		"src/thirdparty/DirectXTex/*.cpp",
		"src/thirdparty/DirectXTex/**.h",
		"src/thirdparty/DirectXTex/**.cpp",
	}))

	includedirs(RootPathList({
		"src/thirdparty/DirectXTex",
		"src/thirdparty/DirectXTex/Shaders/Compiled",
	}))

project "Lua"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")

	files(RootPathList({
		"src/thirdparty/lua/*.h",
		"src/thirdparty/lua/*.c",
	}))

	includedirs(RootPathList({
		"src/thirdparty/lua",
	}))
