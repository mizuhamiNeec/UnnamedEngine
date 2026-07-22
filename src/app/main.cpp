#include <pch.h>

#include <optional>
#include <vector>

#include "GameProfileLoader.h"
#include "GameRuntimeModuleRegistration.h"
#include "LaunchDesc.h"
#include "LoadedGameModule.h"

#include "engine/Engine.h"
#include "engine/game/GameModuleRegistry.h"

namespace Unnamed {
	namespace {
		/// @brief 使用可能なモジュールのリストをカンマ区切りで作成します。
		/// @param moduleNames モジュール名のリスト
		/// @return カンマ区切りのモジュール名の文字列。
		[[nodiscard]] std::string BuildAvailableModulesText(
			const std::vector<std::string>& moduleNames
		) {
			if (moduleNames.empty()) {
				return "<none>";
			}

			std::string text = moduleNames.front();
			for (size_t i = 1; i < moduleNames.size(); ++i) {
				text += ", ";
				text += moduleNames[i];
			}
			return text;
		}
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
#ifdef UNNAMED_WITH_EDITOR
		Unnamed::PrintLaunchHelp("UnnamedEditorApp.exe");
#else
		Unnamed::PrintLaunchHelp("UnnamedLauncher.exe");
#endif
		return EXIT_SUCCESS;
	}

#ifdef UNNAMED_WITH_EDITOR
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedEditorApp", launchOptions);
#else
	Unnamed::EmitLaunchOptionDiagnostics("UnnamedLauncher", launchOptions);
#endif

	Unnamed::GameModuleRegistry moduleRegistry;
	Unnamed::RegisterBuiltInGameModules(moduleRegistry);

	std::string                  requestedModuleName  = {};
	std::optional<Unnamed::Path> selectedManifestPath = std::nullopt;
	// 明示指定されたプロフィールを起動モジュール選択の最優先にする
	if (launchOptions.projectManifestPath.has_value()) {
		if (!Unnamed::GameProfileLoader::ResolveRuntimeModuleFromProfile(
			*launchOptions.projectManifestPath,
			requestedModuleName
		)) {
			return EXIT_FAILURE;
		}
		selectedManifestPath = *launchOptions.projectManifestPath;
	}

	// プロフィール未指定時のみコマンドラインのモジュール名を採用する
	if (requestedModuleName.empty() && launchOptions.gameName.has_value()) {
		requestedModuleName = *launchOptions.gameName;
	}

	if (requestedModuleName.empty() &&
	    !launchOptions.projectManifestPath.has_value() &&
	    !launchOptions.gameName.has_value()) {
		// 起動指定がない場合は既定プロフィールから実行時モジュールを解決する
		Unnamed::Path                                     resolvedManifestPath = {};
		const Unnamed::DefaultGameProfileResolutionResult profileResult        =
			Unnamed::GameProfileLoader::ResolveRuntimeModuleFromDefaultProfile(
				requestedModuleName,
				&resolvedManifestPath
			);
		if (
			profileResult ==
			Unnamed::DefaultGameProfileResolutionResult::Failed
		) {
			return EXIT_FAILURE;
		}
		if (
			profileResult ==
			Unnamed::DefaultGameProfileResolutionResult::Resolved
		) {
			selectedManifestPath = resolvedManifestPath;
		}
	}

	if (requestedModuleName.empty()) {
		const std::vector<std::string> registeredNames =
			moduleRegistry.ListRegisteredNames();
		// 単一モジュール構成だけは明示指定なしでも安全に起動できる
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
			Unnamed::BuildAvailableModulesText(registeredNames)
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
			Unnamed::BuildAvailableModulesText(registeredNames)
		);
		return EXIT_FAILURE;
	}

	if (selectedManifestPath.has_value()) {
		if (!Unnamed::GameProfileLoader::ApplyRuntimeContextFromProfile(
			*selectedManifestPath,
			loadedGameModule->GetRuntimeContext()
		)) {
			return EXIT_FAILURE;
		}
	}

	Msg(
		"Launcher",
		"Selected runtime module '{}' (GameModule='{}').",
		requestedModuleName,
		loadedGameModule->GetGameModuleName()
	);

#ifdef UNNAMED_WITH_EDITOR
	constexpr auto runMode = Unnamed::RUN_MODE::EDITOR;
#else
	constexpr auto runMode = Unnamed::RUN_MODE::STANDALONE;
#endif

	// Engine 初期化から終了までゲーム固有サービスを公開する
	loadedGameModule->RegisterRuntimeContextService();
	// 実体の所有権は loadedGameModule に残し、Engine には実行時の参照だけを渡す
	Unnamed::EngineRuntimeBindings runtimeBindings = {
		.gameWorldFactory  = &loadedGameModule->GetWorldFactory(),
		.runtimeContext    = &loadedGameModule->GetRuntimeContext(),
		.createDemoService = [&] {
			return loadedGameModule->CreateDemoService();
		},
		.sceneLoadOptions = {
			.assetValidationPolicy =
				runMode == Unnamed::RUN_MODE::STANDALONE ||
				launchOptions.validateStartupOnly ?
					Unnamed::SceneAssetValidationPolicy::Strict :
					Unnamed::SceneAssetValidationPolicy::Permissive,
		},
		.renderStartupOptions = {
			.validationPolicy = launchOptions.validateStartupOnly ?
				Unnamed::Render::RENDER_STARTUP_VALIDATION_POLICY::Strict :
				Unnamed::Render::RENDER_STARTUP_VALIDATION_POLICY::Runtime,
		},
		.enableAudioOutput = !launchOptions.disableAudio,
	};
	Unnamed::Engine                   engine(runtimeBindings, runMode);
	const Unnamed::EngineRunCallbacks callbacks = {
		.onPostInitialize = [&](Unnamed::Engine& runningEngine) {
			// エンジン側の共通登録完了後にゲーム固有の登録とロードを行う
			if (!loadedGameModule->RegisterAndLoad(runningEngine)) {
				return false;
			}
			if (launchOptions.validateStartupOnly) {
				Msg(
					"Launcher",
					"validate-startup-only succeeded for runtime module '{}'",
					requestedModuleName
				);
				runningEngine.RequestShutdown();
			}
			return true;
		},
		.onPreShutdown = [&](Unnamed::Engine& runningEngine) {
			// Engine の所有物が破棄される前にゲーム側の解放処理を完了する
			loadedGameModule->Unload(runningEngine);
		},
	};
	const int result = engine.Run(callbacks);
	// Engine の終了後に借用先となるゲームサービスを登録解除する
	loadedGameModule->UnregisterRuntimeContextService();
	return result;
}
