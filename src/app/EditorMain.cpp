#include <pch.h>

#include <engine/Engine.h>
#include <engine/unnamed/subsystem/console/Log.h>

#include "AppLaunchOptions.h"
#include "GameModuleFactory.h"
#ifdef UNNAMED_WITH_PARKOUR_RUNTIME
#include "game/parkour/runtime/ParkourGameModule.h"
#endif
#ifdef UNNAMED_WITH_TEAMGAME_RUNTIME
#include "game/team/runtime/TeamGameModule.h"
#endif

namespace {
	[[nodiscard]] constexpr bool HasLinkedRuntimeModules() {
#if defined(UNNAMED_WITH_PARKOUR_RUNTIME) || defined(UNNAMED_WITH_TEAMGAME_RUNTIME)
		return true;
#else
		return false;
#endif
	}

	[[nodiscard]] bool RegisterEditorRuntimeModules() {
#ifdef UNNAMED_WITH_PARKOUR_RUNTIME
		if (!Unnamed::RegisterGameModule(
			"Parkour",
			&Unnamed::CreateParkourGameModule
		)) {
			Error(
				"EditorApp",
				"Failed to register Parkour runtime module."
			);
			return false;
		}
#endif
#ifdef UNNAMED_WITH_TEAMGAME_RUNTIME
		if (!Unnamed::RegisterGameModule(
			"TeamGame",
			&Unnamed::CreateTeamGameModule
		)) {
			Error(
				"EditorApp",
				"Failed to register TeamGame runtime module."
			);
			return false;
		}
#endif

#ifdef UNNAMED_WITH_PARKOUR_RUNTIME
		(void)Unnamed::RegisterGameModuleAlias("ParkourGame", "Parkour");
#endif
#ifdef UNNAMED_WITH_TEAMGAME_RUNTIME
		(void)Unnamed::RegisterGameModuleAlias("Team", "TeamGame");
#endif
#if !defined(UNNAMED_WITH_PARKOUR_RUNTIME) && !defined(UNNAMED_WITH_TEAMGAME_RUNTIME)
		Warning(
			"EditorApp",
			"No linked game runtime module is available. Editor will run with manifest-only DefaultGameModule."
		);
#endif
		return true;
	}

	[[nodiscard]] std::string ResolveEditorGameName(
		const Unnamed::AppLaunchOptions& launchOptions
	) {
		if (!launchOptions.gameName.has_value() || launchOptions.gameName->empty()) {
#ifdef UNNAMED_WITH_PARKOUR_RUNTIME
			return "Parkour";
#elifdef UNNAMED_WITH_TEAMGAME_RUNTIME
			return "TeamGame";
#else
			return {};
#endif
		}
		return *launchOptions.gameName;
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, const PWSTR lpCmdLine, int) {
	(void)lpCmdLine;

	const Unnamed::AppLaunchOptions launchOptions =
		Unnamed::ParseAppLaunchOptionsFromCommandLine();
	if (launchOptions.showHelp) {
		Unnamed::PrintLaunchHelp("UnnamedEditorApp.exe");
		return EXIT_SUCCESS;
	}
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedEditorApp", launchOptions);

	if (launchOptions.projectManifestPath.has_value()) {
		Unnamed::SetGameModuleManifestPathOverride(
			*launchOptions.projectManifestPath
		);
	}

	if (launchOptions.repoRootOverride.has_value()) {
		Unnamed::SetGameModuleManifestRepoRootOverride(
			*launchOptions.repoRootOverride
		);
	}
	if (launchOptions.projectsRootOverride.has_value()) {
		Unnamed::SetGameModuleManifestProjectsRootOverride(
			*launchOptions.projectsRootOverride
		);
	}

	// Editor は複数ゲーム Runtime をリンクし、選択ゲームの実体モジュールを生成する。
	Unnamed::RegisterDefaultGameModuleProfiles();
	if (!RegisterEditorRuntimeModules()) {
		return EXIT_FAILURE;
	}

	std::string gameName = ResolveEditorGameName(launchOptions);
	if (gameName.empty()) {
		gameName = Unnamed::ResolveSingleRegisteredGameName();
	}
	if (!gameName.empty() && !launchOptions.projectManifestPath.has_value()) {
		(void)Unnamed::PinGameModuleManifestToGame(gameName);
	}
	if (gameName.empty()) {
		Fatal(
			"EditorApp",
			"No game profile was selected. Pass --project=<game_profile.json> or --game=<name> and provide manifest roots via --projects-root, UNNAMED_PROJECTS_ROOT, or --repo-root."
		);
		return EXIT_FAILURE;
	}

	std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateGameModule(gameName);
	if (!gameModule) {
		Fatal(
			"EditorApp",
			"Profile mismatch: game '{}' was not found in the resolved manifest set. Verify --project path or --game alias.",
			gameName
		);
		return EXIT_FAILURE;
	}

#ifdef _DEBUG
	constexpr bool failOnUnknown = HasLinkedRuntimeModules();
#else
	constexpr bool failOnUnknown = false;
#endif
	const bool validated = Unnamed::ValidateGameModuleStartupProfile(
		*gameModule,
		{
			.failOnUnknownComponentTypes = failOnUnknown,
			.emitDetailedLogs            = true,
		}
	);
	if (!validated) {
		return EXIT_FAILURE;
	}
	if (launchOptions.validateStartupOnly) {
		return EXIT_SUCCESS;
	}

	Unnamed::Engine engine(*gameModule, Unnamed::RUN_MODE::EDITOR);
	return engine.Run();
}
