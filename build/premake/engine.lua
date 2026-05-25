group "Engine/Runtime"

project "UnnamedEngineRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files(RootPathList({
		"src/pch.h",
		"src/pch.cpp",
		"src/core/**.h",
		"src/core/**.cpp",
		"src/engine/**.h",
		"src/engine/**.cpp",
		"content/**.hlsl",
		"content/**.hlsli",
	}))

	filter { "files:**.hlsl" }
		excludefrombuild "On"
	filter { "files:**.hlsli" }
		excludefrombuild "On"
	filter {}

	excludes(RootPathList({
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
		"src/engine/game/GameModuleRegistry.cpp",
		"src/app/**",
	}))

	EngineIncludeDirs()
	libdirs { RootPath("src/thirdparty/DirectXTex/") }
	links { "DirectXTex", "Lua" }
	LinkAssimpByConfig()
	filter {}

project "UnnamedEngineRuntimeEditor"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files(RootPathList({
		"src/pch.h",
		"src/pch.cpp",
		"src/core/**.h",
		"src/core/**.cpp",
		"src/engine/**.h",
		"src/engine/**.cpp",
		"content/**.hlsl",
		"content/**.hlsli",
	}))

	filter { "files:**.hlsl" }
		excludefrombuild "On"
	filter { "files:**.hlsli" }
		excludefrombuild "On"
	filter {}

	excludes(RootPathList({
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
		"src/engine/game/GameModuleRegistry.cpp",
		"src/app/**",
	}))

	EngineIncludeDirs()
	libdirs { RootPath("src/thirdparty/DirectXTex/") }
	links { "DirectXTex", "Lua" }
	LinkAssimpByConfig()
	defines { "UNNAMED_WITH_EDITOR" }

project "UnnamedEditorRuntime"
	kind "StaticLib"
	CommonProjectSettings("%{prj.name}")
	WarningSettings()

	files(RootPathList({
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
	}))

	EngineIncludeDirs()

	filter { "files:**/ImGui/**.cpp or files:**/ImGuizmo/**.cpp" }
		warnings "Extra"
		disablewarnings { "4189" }
	filter {}

	defines { "UNNAMED_WITH_EDITOR" }
	filter {}
