#include <pch.h>

#include <filesystem>
#include <vector>

#include "GameRuntimeModuleRegistration.h"
#include "LaunchDesc.h"
#include "LoadedGameModule.h"

#include "core/io/json/JsonReader.h"
#include "engine/Engine.h"
#include "engine/game/GameModuleRegistry.h"

namespace {
	/// @brief 使用可能なモジュールのリストをカンマ区切りで作成します。
	/// @param moduleNames モジュール名のリスト
	/// @return カンマ区切りのモジュール名の文字列。
	[[nodiscard]] std::string BuildAvailableModulesText(
		const std::vector<std::string>& moduleNames
	) {
		if (moduleNames.empty()) { return "<none>"; }

		std::string text = moduleNames.front();
		for (size_t i = 1; i < moduleNames.size(); ++i) {
			text += ", ";
			text += moduleNames[i];
		}
		return text;
	}

	/// @brief ゲームプロファイルからランタイムモジュール名を解決します。
	/// @param manifestPath ゲームプロファイルのパス
	/// @param outRuntimeModule 解決されたランタイムモジュール名の出力先
	/// @return 成功した場合はtrue、失敗した場合はfalse。
	[[nodiscard]] bool ResolveRuntimeModuleFromProfile(
		const std::filesystem::path& manifestPath,
		std::string&                 outRuntimeModule
	) {
		const Unnamed::JsonReader profileReader(manifestPath.generic_string());
		if (!profileReader.Valid()) {
			Error(
				"Launcher",
				"Failed to read game profile '{}'.",
				manifestPath.generic_string()
			);
			return false;
		}

		std::string runtimeModule = profileReader["runtimeModule"].
			GetString("");
		if (runtimeModule.empty()) {
			runtimeModule = profileReader["gameName"].GetString("");
		}

		if (runtimeModule.empty()) {
			Error(
				"Launcher",
				"Failed to resolve runtime module from '{}': both runtimeModule and gameName are empty.",
				manifestPath.generic_string()
			);
			return false;
		}

		outRuntimeModule = runtimeModule;
		return true;
	}
}

//-----------------------------------------------------------------------------
// TODO: エンジンとゲームのリポジトリを分けたい。
//		 が、エディタでゲーム固有のコンポーネントを使おうとするとエディタがゲームを知る必要が
//		 ある。
//-----------------------------------------------------------------------------

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
	const Unnamed::LaunchDesc launchOptions =
		Unnamed::ParseAppLaunchOptionsFromCommandLine();
	if (launchOptions.showHelp) {
#if defined(UNNAMED_WITH_EDITOR)
		Unnamed::PrintLaunchHelp("UnnamedEditorApp.exe");
#else
		Unnamed::PrintLaunchHelp("UnnamedLauncher.exe");
#endif
		return EXIT_SUCCESS;
	}

#if defined(UNNAMED_WITH_EDITOR)
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedEditorApp", launchOptions);
#else
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedLauncher", launchOptions);
#endif

	Unnamed::GameModuleRegistry moduleRegistry;
	Unnamed::RegisterBuiltInGameModules(moduleRegistry);

	std::string requestedModuleName = {};
	if (launchOptions.projectManifestPath.has_value()) {
		if (!ResolveRuntimeModuleFromProfile(
			*launchOptions.projectManifestPath,
			requestedModuleName
		)) { return EXIT_FAILURE; }
	}

	if (requestedModuleName.empty() && launchOptions.gameName.has_value()) {
		requestedModuleName = *launchOptions.gameName;
	}

	if (requestedModuleName.empty()) {
		const std::vector<std::string> registeredNames =
			moduleRegistry.ListRegisteredNames();
		if (registeredNames.size() == 1) {
			requestedModuleName = registeredNames.front();
		}
	}

	if (requestedModuleName.empty()) {
		const std::vector<std::string> registeredNames =
			moduleRegistry.ListRegisteredNames();
		Fatal(
			"Launcher",
			"No runtime module was selected. Pass --project=<game_profile.json> with runtimeModule, or --game=<module>. Registered modules: {}",
			BuildAvailableModulesText(registeredNames)
		);
		return EXIT_FAILURE;
	}

	std::unique_ptr<Unnamed::LoadedGameModule> loadedGameModule =
		Unnamed::LoadedGameModule::Create(
			moduleRegistry,
			requestedModuleName
		);
	if (!loadedGameModule) {
		const std::vector<std::string> registeredNames =
			moduleRegistry.ListRegisteredNames();
		Fatal(
			"Launcher",
			"Runtime module '{}' is not registered. Registered modules: {}",
			requestedModuleName,
			BuildAvailableModulesText(registeredNames)
		);
		return EXIT_FAILURE;
	}

	Msg(
		"Launcher",
		"Selected runtime module '{}' (GameModule='{}').",
		requestedModuleName,
		loadedGameModule->GetGameModuleName()
	);

	if (launchOptions.validateStartupOnly) {
		Msg(
			"Launcher",
			"validate-startup-only succeeded for runtime module '{}'",
			requestedModuleName
		);
		return EXIT_SUCCESS;
	}

#if defined(UNNAMED_WITH_EDITOR)
	constexpr auto runMode = Unnamed::RUN_MODE::EDITOR;
#else
	constexpr auto runMode = Unnamed::RUN_MODE::STANDALONE;
#endif

	loadedGameModule->RegisterRuntimeContextService();
	Unnamed::EngineRuntimeBindings runtimeBindings = {
		.gameWorldFactory  = &loadedGameModule->GetWorldFactory(),
		.runtimeContext    = &loadedGameModule->GetRuntimeContext(),
		.createDemoService = [&] {
			return loadedGameModule->CreateDemoService();
		},
	};
	Unnamed::Engine                   engine(runtimeBindings, runMode);
	const Unnamed::EngineRunCallbacks callbacks = {
		.onPostInitialize = [&](Unnamed::Engine& runningEngine) {
			return loadedGameModule->RegisterAndLoad(runningEngine);
		},
		.onPreShutdown = [&](Unnamed::Engine& runningEngine) {
			loadedGameModule->Unload(runningEngine);
		},
	};
	const int result = engine.Run(callbacks);
	loadedGameModule->UnregisterRuntimeContextService();
	return result;
}
