ENGINE_NAME = "Unnamed"

ARCHITECTURE = "x64"

ROOT_DIR = path.getabsolute(".")
BIN_DIR = path.join(ROOT_DIR, "bin")
INT_DIR = path.join(BIN_DIR, "intermediate")

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

function UnnamedSettings()
	defines {
		"ENGINE_NAME=\"" .. ENGINE_NAME .. "\"",
		"ENGINE_VERSION=\"0.1.0\"",
		"_CRTDBG_MAP_ALLOC",
	}
end

function CommonSettings()
    language "C++"
    cppdialect "C++23"
    architecture(ARCHITECTURE)
    flags { "MultiProcessorCompile" }
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
        -- defines { "WIN32_LEAN_AND_MEAN" }
    filter {}
end

function PCHSettings()
	pchheader "pch.h"
	pchsource "src/pch.cpp"
end

workspace(ENGINE_NAME)
    configurations { "Debug", "Develop", "Release" }

group "Thirdparty"
project "DirectXTex"
	kind "StaticLib"
	language "C++"
	
	ConfigurationSettings()
	CommonSettings()
	
	targetdir(path.join(BIN_DIR, outputdir, "%{prj.name}"))
	objdir(path.join(INT_DIR, outputdir, "%{prj.name}"))
	
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

group "Engine"
    project(ENGINE_NAME)
        kind "WindowedApp"
        CommonSettings()
        WarningSettings()
        ConfigurationSettings()
        UnnamedSettings()
        WindowsPlatformSettings()

        targetdir(path.join(BIN_DIR, outputdir, "%{prj.name}"))
        objdir(path.join(INT_DIR, outputdir, "%{prj.name}"))

        files {
            "src/**.h",
            "src/**.cpp",
            "src/**.h",
            "src/**.cpp",
            "content/**.hlsl",
            "content/**.hlsli",
        }
	
		filter { "files:content/**.hlsl" }
			flags { "ExcludeFromBuild" }
			
		filter { "files:content/**.hlsli" }
			flags { "ExcludeFromBuild" }
			
		filter {}
	
		excludes {
			"src/thirdparty/**",
		}

        includedirs {
            "src/",
        }
	
		includedirs {
			"src/thirdparty/assimp/include",
			"src/thirdparty/DirectXTex",
			"src/thirdparty/ImGui",
			"src/thirdparty/ImGuizmo",
			"src/thirdparty/nlohmann",
		}
	
		libdirs {
			"src/thirdparty/DirectXTex/"
		}
	
		links {
			"DirectXTex",
		}
	
	    filter "configurations:Debug"
	    links {
			"src/thirdparty/assimp/lib/Debug/assimp-vc143-mdd.lib",
		}
		files {
        	"src/thirdparty/ImGui/imgui.cpp",
    	    "src/thirdparty/ImGui/imgui_draw.cpp",
    	    "src/thirdparty/ImGui/imgui_widgets.cpp",
    	    "src/thirdparty/ImGui/imgui_tables.cpp",
    	    "src/thirdparty/ImGui/imgui_demo.cpp",
    	    "src/thirdparty/ImGui/imgui_impl_dx12.cpp",
    	    "src/thirdparty/ImGui/imgui_impl_win32.cpp",
    	
    	    "src/thirdparty/ImGuizmo/ImGuizmo.cpp",
    	}
	
        filter "configurations:Develop"
        links {
			"src/thirdparty/assimp/lib/Release/assimp-vc143-md.lib",
		}
    
        filter "configurations:Release"
        links {
			"src/thirdparty/assimp/lib/Release/assimp-vc143-md.lib",
		}
            
        filter {}
	
		postbuildcommands {
            'copy /Y "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxcompiler.dll" "%{cfg.targetdir}\\dxcompiler.dll"',
            'copy /Y "$(WindowsSdkDir)bin\\$(TargetPlatformVersion)\\x64\\dxil.dll" "%{cfg.targetdir}\\dxil.dll"'
        }
	
		filter {}