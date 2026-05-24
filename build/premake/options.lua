newoption {
	trigger = "games",
	value = "LIST",
	description = "[Deprecated] Ignored. Game runtime/app selection now belongs to the game repository.",
}

newoption {
	trigger = "projects-root",
	value = "PATH",
	description = "Root directory that contains game projects (e.g. S:/Repositories/TD4_01/projects).",
}

newoption {
	trigger = "startup-app",
	value = "APP",
	description = "Default startup app for generated solution. Values: editor, game.",
}

newoption {
	trigger = "startup-project",
	value = "PATH",
	description = "Default --project path injected into debugger command arguments.",
}

local requestedProjectsRoot = _OPTIONS["projects-root"] or os.getenv("UNNAMED_GAME_PROJECTS_ROOT") or "projects"
if path.isabsolute(requestedProjectsRoot) then
	PROJECTS_ROOT = requestedProjectsRoot
else
	PROJECTS_ROOT = path.getabsolute(path.join(ROOT_DIR, requestedProjectsRoot))
end

local requestedStartupApp = (_OPTIONS["startup-app"] or os.getenv("UNNAMED_STARTUP_APP") or "editor"):lower()
if requestedStartupApp == "game" or requestedStartupApp == "launcher" then
	STARTUP_PROJECT = "UnnamedLauncher"
else
	STARTUP_PROJECT = "UnnamedEditorApp"
end

local requestedStartupProjectPath = _OPTIONS["startup-project"] or os.getenv("UNNAMED_STARTUP_PROJECT")
if requestedStartupProjectPath == nil or requestedStartupProjectPath == "" then
	requestedStartupProjectPath = "projects/ParkourGame/config/game_profile.json"
end

if path.isabsolute(requestedStartupProjectPath) then
	STARTUP_PROJECT_MANIFEST = path.getrelative(ROOT_DIR, requestedStartupProjectPath)
else
	STARTUP_PROJECT_MANIFEST = requestedStartupProjectPath
end
