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
		Error(
			"EditorApp",
			"No linked game runtime module is available. Check premake game selection."
		);
		return false;
#endif
		return true;
	}

	[[nodiscard]] std::string ResolveEditorGameName(
		const Unnamed::AppLaunchOptions& launchOptions
	) {
		if (!launchOptions.gameName.has_value() || launchOptions.gameName->empty()) {
#ifdef UNNAMED_WITH_PARKOUR_RUNTIME
			return "Parkour";
#elif defined(UNNAMED_WITH_TEAMGAME_RUNTIME)
			return "TeamGame";
#else
			return {};
#endif
		}
		return *launchOptions.gameName;
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
	(void)commandLine;

	const Unnamed::AppLaunchOptions launchOptions =
		Unnamed::ParseAppLaunchOptionsFromCommandLine();
	if (launchOptions.showHelp) {
		Unnamed::PrintLaunchHelp("UnnamedEditorApp.exe");
		return EXIT_SUCCESS;
	}
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedEditorApp", launchOptions);

	if (launchOptions.repoRootOverride.has_value()) {
		Unnamed::SetGameModuleManifestRepoRootOverride(
			*launchOptions.repoRootOverride
		);
	}

	// Editor は複数ゲーム Runtime をリンクし、選択ゲームの実体モジュールを生成する。
	Unnamed::RegisterDefaultGameModuleProfiles();
	if (!RegisterEditorRuntimeModules()) {
		return EXIT_FAILURE;
	}

	const std::string gameName = ResolveEditorGameName(launchOptions);
	std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateGameModule(gameName);
	if (!gameModule) {
#ifdef UNNAMED_WITH_PARKOUR_RUNTIME
		Error("EditorApp", "Unknown game profile '{}'. Fallback to Parkour", gameName);
		gameModule = Unnamed::CreateGameModule("Parkour");
		if (!gameModule) {
			Fatal("EditorApp", "Failed to create fallback Parkour game module.");
			return EXIT_FAILURE;
		}
#elif defined(UNNAMED_WITH_TEAMGAME_RUNTIME)
		Error("EditorApp", "Unknown game profile '{}'. Fallback to TeamGame", gameName);
		gameModule = Unnamed::CreateGameModule("TeamGame");
		if (!gameModule) {
			Fatal("EditorApp", "Failed to create fallback TeamGame game module.");
			return EXIT_FAILURE;
		}
#else
		Fatal("EditorApp", "Unknown game profile '{}'. No runtime module linked.", gameName);
		return EXIT_FAILURE;
#endif
	}

#ifdef _DEBUG
	constexpr bool failOnUnknown = true;
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
