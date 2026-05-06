#include <pch.h>

#include <engine/Engine.h>

#include "AppLaunchOptions.h"
#include "GameModuleFactory.h"
#include "game/parkour/runtime/ParkourGameModule.h"

namespace {
	[[nodiscard]] bool RegisterParkourRuntimeModule() {
		// App がリンク済みの Parkour モジュールを Factory へ注入する。
		if (!Unnamed::RegisterGameModule(
			"Parkour",
			&Unnamed::CreateParkourGameModule
		)) {
			return false;
		}
		(void)Unnamed::RegisterGameModuleAlias("ParkourGame", "Parkour");
		return true;
	}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	const Unnamed::AppLaunchOptions launchOptions =
		Unnamed::ParseAppLaunchOptionsFromCommandLine();
	if (launchOptions.showHelp) {
		Unnamed::PrintLaunchHelp("ParkourGameApp.exe");
		return EXIT_SUCCESS;
	}
	Unnamed::EmitLaunchOptionDiagnostics("ParkourGameApp", launchOptions);

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

	if (!RegisterParkourRuntimeModule()) {
		Error("ParkourGameApp", "Failed to register Parkour game module profile.");
		return EXIT_FAILURE;
	}
	if (!launchOptions.projectManifestPath.has_value()) {
		(void)Unnamed::PinGameModuleManifestToGame("Parkour");
	}
	std::unique_ptr<Unnamed::IGameModule> gameModule =
		Unnamed::CreateGameModule("Parkour");
	if (!gameModule) {
		Error(
			"ParkourGameApp",
			"Profile mismatch: game '{}' was not found in the resolved manifest set. Verify --project path or manifest roots.",
			"Parkour"
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
