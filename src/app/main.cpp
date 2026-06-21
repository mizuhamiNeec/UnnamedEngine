#include <pch.h>

#include <filesystem>
#include <optional>
#include <vector>

#include "GameRuntimeModuleRegistration.h"
#include "LaunchDesc.h"
#include "LoadedGameModule.h"

#include "core/io/json/JsonReader.h"
#include "core/filesystem/Path.h"

#include "engine/Engine.h"
#include "engine/game/GameRuntimeContext.h"
#include "engine/game/GameModuleRegistry.h"

namespace {
	/// @brief パスを絶対パスへ解決し、正規化します。
	/// @details 失敗した場合は入力値の lexical 正規化結果へフォールバックします。
	[[nodiscard]] Unnamed::Path ResolveAbsoluteNormalizedPath(
		const Unnamed::Path& path
	) {
		std::error_code             ec           = {};
		const std::filesystem::path absolutePath = std::filesystem::absolute(
			path.Native(),
			ec
		);
		if (ec) {
			return path.LexicallyNormal();
		}
		return Unnamed::Path::FromNative(absolutePath.lexically_normal());
	}

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

	/// @brief ゲームプロファイルからランタイムモジュール名を解決します。
	/// @param manifestPath ゲームプロファイルのパス
	/// @param outRuntimeModule 解決されたランタイムモジュール名の出力先
	/// @return 成功した場合はtrue、失敗した場合はfalse。
	[[nodiscard]] bool ResolveRuntimeModuleFromProfile(
		const Unnamed::Path& manifestPath,
		std::string&         outRuntimeModule
	) {
		const Unnamed::Path normalizedManifestPath =
			ResolveAbsoluteNormalizedPath(manifestPath);
		const Unnamed::JsonReader profileReader(normalizedManifestPath);
		if (!profileReader.Valid()) {
			Error(
				"Launcher",
				"Failed to read game profile '{}'.",
				normalizedManifestPath
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
				normalizedManifestPath
			);
			return false;
		}

		outRuntimeModule = runtimeModule;
		return true;
	}

	[[nodiscard]] Unnamed::Path ResolveProfilePathField(
		const Unnamed::JsonReader& profileReader,
		const Unnamed::Path&       manifestPath,
		const std::string_view     fieldName,
		const Unnamed::Path&       fallbackPath
	) {
		const std::string rawPath =
			profileReader[std::string(fieldName)].GetString("");
		if (rawPath.empty()) {
			return fallbackPath;
		}

		const auto valuePath = Unnamed::Path(rawPath);
		if (valuePath.IsAbsolute()) {
			return valuePath.LexicallyNormal();
		}

		return (manifestPath.ParentPath() / valuePath).LexicallyNormal();
	}

	[[nodiscard]] bool ApplyRuntimeContextFromProfile(
		const Unnamed::Path&       manifestPath,
		Unnamed::LoadedGameModule& loadedGameModule
	) {
		const Unnamed::Path normalizedManifestPath =
			ResolveAbsoluteNormalizedPath(manifestPath);
		const Unnamed::JsonReader profileReader(normalizedManifestPath);
		if (!profileReader.Valid()) {
			Error(
				"Launcher",
				"Failed to read game profile '{}'.",
				normalizedManifestPath
			);
			return false;
		}

		Unnamed::GameRuntimeContext& runtimeContext =
			loadedGameModule.GetRuntimeContext();
		Unnamed::GameModulePaths& modulePaths = runtimeContext.modulePaths;

		const std::string gameName = profileReader["gameName"].GetString("");
		if (!gameName.empty()) {
			modulePaths.gameName = gameName;
		}

		modulePaths.gameRoot = ResolveProfilePathField(
			profileReader,
			normalizedManifestPath,
			"gameRoot",
			modulePaths.gameRoot
		);
		modulePaths.contentRoot = ResolveProfilePathField(
			profileReader,
			normalizedManifestPath,
			"contentRoot",
			modulePaths.contentRoot
		);
		modulePaths.configRoot = ResolveProfilePathField(
			profileReader,
			normalizedManifestPath,
			"configRoot",
			modulePaths.configRoot
		);

		modulePaths.defaultStartupScene = Unnamed::Path(
			profileReader["defaultStartupScene"].GetString(
				modulePaths.defaultStartupScene.ToGenericUtf8()
			)
		);
		runtimeContext.defaultStartupScenePath = modulePaths.
			defaultStartupScene;

		modulePaths.runtimeBinaryPath = Unnamed::Path(
			profileReader["runtimeBinary"].GetString(
				modulePaths.runtimeBinaryPath.ToGenericUtf8()
			)
		);
		modulePaths.requireRuntimeBinary =
			profileReader["requireRuntimeBinary"].GetBool(
				modulePaths.requireRuntimeBinary
			);
		modulePaths.preferRuntimeBinary =
			profileReader["preferRuntimeBinary"].GetBool(
				modulePaths.preferRuntimeBinary
			);
		modulePaths.resolvedManifestPath = normalizedManifestPath;

		Msg(
			"Launcher",
			"Applied game profile paths: manifest='{}' gameRoot='{}' contentRoot='{}' configRoot='{}' startupScene='{}'",
			modulePaths.resolvedManifestPath,
			modulePaths.gameRoot,
			modulePaths.contentRoot,
			modulePaths.configRoot,
			runtimeContext.defaultStartupScenePath
		);
		return true;
	}

	enum class DefaultProfileResolutionResult {
		NotFound,
		Resolved,
		Failed,
	};

	/// @brief 実行中 EXE の配置ディレクトリを返します。
	/// @return 解決できた場合はディレクトリパス、失敗時は nullopt。
	[[nodiscard]] std::optional<Unnamed::Path> TryResolveExecutableDirectory() {
		std::vector<wchar_t> buffer(260, L'\0');
		while (true) {
			const DWORD copied = GetModuleFileNameW(
				nullptr,
				buffer.data(),
				static_cast<DWORD>(buffer.size())
			);
			if (copied == 0) {
				return std::nullopt;
			}

			if (copied < buffer.size() - 1) {
				const Unnamed::Path exePath = Unnamed::Path::FromNative(
					std::filesystem::path(std::wstring(
						buffer.data(),
						copied
					)));
				return exePath.ParentPath();
			}

			buffer.resize(buffer.size() * 2, L'\0');
		}
	}

	/// @brief 引数未指定時に既定マニフェストからランタイムモジュールを解決します。
	/// @param outRuntimeModule 解決されたランタイムモジュール名の出力先
	/// @return 解決結果。
	[[nodiscard]] DefaultProfileResolutionResult
	ResolveRuntimeModuleFromDefaultProfile(
		std::string&   outRuntimeModule,
		Unnamed::Path* outResolvedManifestPath
	) {
		static constexpr std::string_view kDefaultManifestRelativePath =
			"config/game_profile.json";

		std::vector<Unnamed::Path>  candidates = {};
		std::error_code             ec = {};
		const std::filesystem::path cwd = std::filesystem::current_path(ec);
		if (!ec) {
			candidates.emplace_back(
				Unnamed::Path::FromNative(cwd) /
				Unnamed::Path(kDefaultManifestRelativePath)
			);
		}

		if (const auto exeDir = TryResolveExecutableDirectory();
			exeDir.has_value()) {
			candidates.emplace_back(
				*exeDir / Unnamed::Path(kDefaultManifestRelativePath)
			);
		}

		std::vector<Unnamed::Path> uniqueCandidates = {};
		uniqueCandidates.reserve(candidates.size());
		for (const auto& candidate : candidates) {
			const Unnamed::Path normalized   = candidate.LexicallyNormal();
			const bool          alreadyAdded =
				std::ranges::find(uniqueCandidates, normalized) !=
				uniqueCandidates.end();
			if (!alreadyAdded) {
				uniqueCandidates.emplace_back(normalized);
			}
		}

		for (const auto& manifestPath : uniqueCandidates) {
			ec = {};
			if (!std::filesystem::exists(manifestPath.Native(), ec) || ec) {
				continue;
			}

			Msg(
				"Launcher",
				"Found default game profile '{}'.",
				manifestPath
			);

			if (!ResolveRuntimeModuleFromProfile(
				manifestPath, outRuntimeModule)) {
				return DefaultProfileResolutionResult::Failed;
			}
			if (outResolvedManifestPath != nullptr) {
				*outResolvedManifestPath = manifestPath;
			}

			return DefaultProfileResolutionResult::Resolved;
		}

		return DefaultProfileResolutionResult::NotFound;
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

	std::string                  requestedModuleName  = {};
	std::optional<Unnamed::Path> selectedManifestPath = std::nullopt;
	if (launchOptions.projectManifestPath.has_value()) {
		if (!ResolveRuntimeModuleFromProfile(
			*launchOptions.projectManifestPath,
			requestedModuleName
		)) {
			return EXIT_FAILURE;
		}
		selectedManifestPath = *launchOptions.projectManifestPath;
	}

	if (requestedModuleName.empty() && launchOptions.gameName.has_value()) {
		requestedModuleName = *launchOptions.gameName;
	}

	if (requestedModuleName.empty() &&
	    !launchOptions.projectManifestPath.has_value() &&
	    !launchOptions.gameName.has_value()) {
		Unnamed::Path                        resolvedManifestPath = {};
		const DefaultProfileResolutionResult profileResult        =
			ResolveRuntimeModuleFromDefaultProfile(
				requestedModuleName,
				&resolvedManifestPath
			);
		if (profileResult == DefaultProfileResolutionResult::Failed) {
			return EXIT_FAILURE;
		}
		if (profileResult == DefaultProfileResolutionResult::Resolved) {
			selectedManifestPath = resolvedManifestPath;
		}
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

	if (selectedManifestPath.has_value()) {
		if (!ApplyRuntimeContextFromProfile(
			*selectedManifestPath,
			*loadedGameModule
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

	if (launchOptions.validateStartupOnly) {
		Msg(
			"Launcher",
			"validate-startup-only succeeded for runtime module '{}'",
			requestedModuleName
		);
		return EXIT_SUCCESS;
	}

#ifdef UNNAMED_WITH_EDITOR
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
