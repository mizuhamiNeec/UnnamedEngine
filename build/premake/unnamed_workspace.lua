-- ゲームプロジェクト側から呼ばれ、エンジンのプロジェクトファイルを生成します。
function CreateUnnamedWorkspace(options)
    local resolvedEngineRoot = options.engineRoot or options.rootDir
    if not resolvedEngineRoot then
        error("CreateUnnamedWorkspace: options.engineRoot or options.rootDir is required")
    end

    ENGINE_ROOT = path.getabsolute(resolvedEngineRoot)
    UNNAMED_ROOT_DIR_OVERRIDE = ENGINE_ROOT
    PROJECTS_ROOT = options.projectsRoot
    OUTPUT_ROOT = options.outputRoot or ENGINE_ROOT

    function IncludeEnginePremake(file)
        include(path.join(ENGINE_ROOT, "build/premake", file))
    end

    IncludeEnginePremake("common.lua")
    IncludeEnginePremake("options.lua")

    workspace(ENGINE_NAME)
        configurations { "Debug", "Develop", "Release" }

    IncludeEnginePremake("thirdparty.lua")
    IncludeEnginePremake("engine.lua")
    IncludeEnginePremake("games.lua")
    IncludeEnginePremake("apps.lua")
end
