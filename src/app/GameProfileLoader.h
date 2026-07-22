#pragma once

#include <algorithm>
#include <filesystem>
#include <optional>
#include <vector>

#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"
#include "core/io/json/JsonReader.h"
#include "engine/game/GameRuntimeContext.h"

namespace Unnamed {
	/// @brief 既定 game_profile.json の探索結果です。
	enum class DefaultGameProfileResolutionResult {
		NotFound,
		Resolved,
		Failed,
	};

	namespace GameProfileLoader {
		/// @brief パスを絶対パスへ解決し、正規化します。
		/// @details 失敗した場合は入力値の lexical 正規化結果へフォールバックします。
		[[nodiscard]] inline Path ResolveAbsoluteNormalizedPath(
			const Path& path
		) {
			std::error_code             ec           = {};
			const std::filesystem::path absolutePath =
				std::filesystem::absolute(
					path.Native(),
					ec
				);
			if (ec) {
				return path.LexicallyNormal();
			}
			return Path::FromNative(absolutePath.lexically_normal());
		}

		/// @brief 実行中 EXE の配置ディレクトリを返します。
		/// @return 解決できた場合はディレクトリパス、失敗時は nullopt。
		[[nodiscard]] inline std::optional<Path> TryResolveExecutableDirectory() {
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
					const Path exePath = Path::FromNative(
						std::filesystem::path(std::wstring(
							buffer.data(),
							copied
						)));
					return exePath.ParentPath();
				}

				buffer.resize(buffer.size() * 2, L'\0');
			}
		}

		/// @brief プロファイルのパス項目を manifest 基準で物理 Path として読み込みます。
		[[nodiscard]] inline Path ResolveProfilePathField(
			const JsonReader&      profileReader,
			const Path&            manifestPath,
			const std::string_view fieldName,
			const Path&            fallbackPath
		) {
			const std::string rawPath =
				profileReader[std::string(fieldName)].GetString("");
			if (rawPath.empty()) {
				return fallbackPath;
			}

			const Path valuePath(rawPath);
			if (valuePath.IsAbsolute()) {
				return valuePath.LexicallyNormal();
			}

			return (manifestPath.ParentPath() / valuePath).LexicallyNormal();
		}

		/// @brief game_profile.json から runtime module 名を解決します。
		[[nodiscard]] inline bool ResolveRuntimeModuleFromProfile(
			const Path&  manifestPath,
			std::string& outRuntimeModule
		) {
			const Path normalizedManifestPath =
				ResolveAbsoluteNormalizedPath(manifestPath);
			const JsonReader profileReader(normalizedManifestPath);
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

		/// @brief game_profile.json を RuntimeContext へ適用します。
		/// @details 物理ルートは Path、起動シーンは VirtualPath として保持します。
		[[nodiscard]] inline bool ApplyRuntimeContextFromProfile(
			const Path&         manifestPath,
			GameRuntimeContext& runtimeContext
		) {
			const Path normalizedManifestPath =
				ResolveAbsoluteNormalizedPath(manifestPath);
			const JsonReader profileReader(normalizedManifestPath);
			if (!profileReader.Valid()) {
				Error(
					"Launcher",
					"Failed to read game profile '{}'.",
					normalizedManifestPath
				);
				return false;
			}

			GameModulePaths& modulePaths = runtimeContext.modulePaths;

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

			const std::string defaultStartupSceneUtf8 =
				profileReader["defaultStartupScene"].GetString(
					modulePaths.defaultStartupScene.IsEmpty() ?
					"" :
					modulePaths.defaultStartupScene.String()
				);
			const std::optional<VirtualPath> defaultStartupScene =
				VirtualPath::ParseContentReference(defaultStartupSceneUtf8);
			if (!defaultStartupScene.has_value()) {
				Error(
					"Launcher",
					"Failed to parse defaultStartupScene as VirtualPath: manifest='{}' raw='{}'",
					normalizedManifestPath,
					defaultStartupSceneUtf8
				);
				return false;
			}

			modulePaths.defaultStartupScene = *defaultStartupScene;
			runtimeContext.defaultStartupScene = *defaultStartupScene;

			modulePaths.runtimeBinaryPath = Path(
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
				"Applied game profile: manifest='{}' gameRoot='{}' contentRoot='{}' configRoot='{}' startupScene='{}'",
				modulePaths.resolvedManifestPath,
				modulePaths.gameRoot,
				modulePaths.contentRoot,
				modulePaths.configRoot,
				runtimeContext.defaultStartupScene.String()
			);
			return true;
		}

		/// @brief 引数未指定時に既定マニフェストからランタイムモジュールを解決します。
		[[nodiscard]] inline DefaultGameProfileResolutionResult
		ResolveRuntimeModuleFromDefaultProfile(
			std::string& outRuntimeModule,
			Path*        outResolvedManifestPath
		) {
			static constexpr std::string_view kDefaultManifestRelativePath =
				"config/game_profile.json";

			std::vector<Path>           candidates = {};
			std::error_code             ec = {};
			const std::filesystem::path cwd = std::filesystem::current_path(ec);
			if (!ec) {
				candidates.emplace_back(
					Path::FromNative(cwd) /
					Path(kDefaultManifestRelativePath)
				);
			}

			if (const auto exeDir = TryResolveExecutableDirectory();
				exeDir.has_value()) {
				candidates.emplace_back(
					*exeDir / Path(kDefaultManifestRelativePath)
				);
			}

			std::vector<Path> uniqueCandidates = {};
			uniqueCandidates.reserve(candidates.size());
			for (const auto& candidate : candidates) {
				const Path normalized   = candidate.LexicallyNormal();
				const bool alreadyAdded =
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
					manifestPath,
					outRuntimeModule
				)) {
					return DefaultGameProfileResolutionResult::Failed;
				}

				if (outResolvedManifestPath != nullptr) {
					*outResolvedManifestPath = manifestPath;
				}
				return DefaultGameProfileResolutionResult::Resolved;
			}

			return DefaultGameProfileResolutionResult::NotFound;
		}
	}
}
