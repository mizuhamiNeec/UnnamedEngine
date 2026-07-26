dofile("build/premake/common.lua")
dofile("build/premake/options.lua")

workspace(ENGINE_NAME)
	configurations { "Debug", "Develop", "Release" }

local startupProject = STARTUP_PROJECT or "UnnamedEditorApp"
local parkourRuntimeRoot = path.join(ROOT_DIR, "projects/ParkourGame/runtime")
if startupProject == "UnnamedLauncher" and not os.isdir(parkourRuntimeRoot) then
	startupProject = "UnnamedEditorApp"
end
startproject(startupProject)

dofile("build/premake/thirdparty.lua")
dofile("build/premake/engine.lua")
if not _OPTIONS["engine-only"] then
	dofile("build/premake/apps.lua")
end
dofile("build/premake/tests.lua")
