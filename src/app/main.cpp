#include <pch.h>
#include <engine/Engine.h>

#include "AppLaunchOptions.h"
#include "GameModuleFactory.h"

namespace {
	[[nodiscard]] std::string ResolveStandaloneGameName(
		const Unnamed::AppLaunchOptions& launchOptions
	) {
		if (!launchOptions.gameName.has_value() || launchOptions.gameName->empty()) {
			return {};
		}
		return *launchOptions.gameName;
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR commandLine, int) {
	(void)commandLine;

	const Unnamed::AppLaunchOptions launchOptions =
		Unnamed::ParseAppLaunchOptionsFromCommandLine();
	if (launchOptions.showHelp) {
		Unnamed::PrintLaunchHelp("UnnamedLauncher.exe");
		return EXIT_SUCCESS;
	}
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedLauncher", launchOptions);

	if (launchOptions.repoRootOverride.has_value()) {
		Unnamed::SetGameModuleManifestRepoRootOverride(
			*launchOptions.repoRootOverride
		);
	}

	Unnamed::RegisterDefaultGameModuleProfiles();

	const std::string gameName = ResolveStandaloneGameName(launchOptions);
	if (gameName.empty()) {
		Error(
			"UnnamedLauncher",
			"No game profile was selected. Pass --game=<name> and provide manifests via UNNAMED_PROJECTS_ROOT or --repo-root."
		);
		return EXIT_FAILURE;
	}

	std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateGameModule(gameName);
	if (!gameModule) {
		Error(
			"UnnamedLauncher",
			"Unknown game profile '{}'. Verify --game value and game_profile.json resolution.",
			gameName
		);
		return EXIT_FAILURE;
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
			.emitDetailedLogs = true,
		}
	);
	if (!validated) {
		return EXIT_FAILURE;
	}
	if (launchOptions.validateStartupOnly) {
		return EXIT_SUCCESS;
	}

	Unnamed::Engine engine(*gameModule, Unnamed::RUN_MODE::STANDALONE);
	return engine.Run();
}
