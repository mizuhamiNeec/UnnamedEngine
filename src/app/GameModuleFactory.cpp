#include "GameModuleFactory.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <optional>
#include <queue>
#include <span>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <array>

#include <json.hpp>
#include <Windows.h>

#include "core/ComponentRegistry.h"
#include "engine/EngineComponentRegistration.h"
#include "engine/game/GamePathResolver.h"
#include "engine/game/IDemoService.h"
#include "engine/physics/core/Physics.h"
#include "engine/scene/Scene.h"
#include "engine/unnamed/subsystem/console/Log.h"
#include "engine/world/World.h"

namespace Unnamed {
	namespace {
		constexpr std::string_view kChannel = "GameModuleFactory";
		constexpr std::array<std::string_view, 1> kSupportedEngineApiVersions = {
			"1",
		};
		constexpr std::array<std::string_view, 1> kSupportedGameApiVersions = {
			"1",
		};
		// 段階移行ルール: deprecated は警告付き許容。期限を過ぎたら supported から除外する。
		constexpr std::array<std::string_view, 1> kDeprecatedEngineApiVersions = {
			"0",
		};
		constexpr std::array<std::string_view, 1> kDeprecatedGameApiVersions = {
			"0",
		};
		constexpr std::string_view kEngineApiDeprecationNote =
			"engineApi '0' is deprecated and scheduled for removal after 2026-09-30.";
		constexpr std::string_view kGameApiDeprecationNote =
			"gameApi '0' is deprecated and scheduled for removal after 2026-09-30.";

		enum class ApiCompatibilityLevel {
			Supported,
			Deprecated,
			Unsupported,
		};

		struct ApiCompatibilityResult {
			ApiCompatibilityLevel level = ApiCompatibilityLevel::Unsupported;
			std::string           message;
		};

		struct RegisteredGameModule {
			std::optional<GameModulePaths> paths;
			GameModuleCreateFunction createFunction = nullptr;
		};

		struct LoadedRuntimeLibrary {
			HMODULE                moduleHandle = nullptr;
			const GameRuntimeApiV1* api = nullptr;
			std::string            runtimeBinaryPath;
		};

		enum class RuntimeLoadFailure {
			None,
			RuntimePathEmpty,
			RuntimeBinaryNotFound,
			RuntimeBinaryNotFile,
			LoadLibraryFailed,
			MissingRuntimeSymbol,
			InvalidRuntimeApi,
		};

		struct RuntimeLoadResult {
			LoadedRuntimeLibrary* runtimeLibrary = nullptr;
			RuntimeLoadFailure    failure = RuntimeLoadFailure::None;
			std::string           runtimePath;
			DWORD                 systemErrorCode = 0;
			std::uint32_t         apiVersion = 0;
			std::uint32_t         apiStructSize = 0;
		};

		struct LoadedGameProfile {
			GameModulePaths            paths;
			std::vector<std::string> aliases;
			std::string               modsRootOverride;
			std::vector<std::string> enabledMods;
			std::vector<std::string> disabledMods;
		};

		struct ModDependencySpec {
			std::string id;
			std::string versionConstraint;
		};

		struct LoadedModManifest {
			std::string                    id;
			std::string                    version;
			std::string                    engineApi;
			std::string                    gameApi;
			std::string                    contentRoot;
			std::filesystem::path          modRootPath;
			std::filesystem::path          manifestPath;
			std::vector<ModDependencySpec> dependencies;
		};

		struct ManifestLoadResult {
			std::optional<LoadedGameProfile> profile;
			std::string                      failureReason;
		};

		struct ManifestSearchConfiguration {
			std::optional<std::filesystem::path> explicitRepoRootOverride;
			std::optional<std::filesystem::path> explicitProjectsRootOverride;
			std::optional<std::filesystem::path> explicitManifestPathOverride;
		};

		struct GameModuleRegistryState {
			std::unordered_map<std::string, RegisteredGameModule> modulesByName;
			std::unordered_map<std::string, std::string> aliasToCanonical;
			std::unordered_map<std::string, LoadedRuntimeLibrary> loadedRuntimeLibraries;
			ManifestSearchConfiguration manifestSearch = {};
			bool defaultsRegistered = false;
		};

		struct RuntimeBindingSnapshot {
			std::unordered_map<std::string, GameModuleCreateFunction> createFunctions;
			std::vector<std::pair<std::string, std::string>>          aliases;
		};

		[[nodiscard]] GameModuleRegistryState& GetRegistryState() {
			static GameModuleRegistryState state;
			return state;
		}

		void UnloadRuntimeLibrary(LoadedRuntimeLibrary& runtimeLibrary) {
			if (runtimeLibrary.moduleHandle != nullptr) {
				::FreeLibrary(runtimeLibrary.moduleHandle);
				runtimeLibrary.moduleHandle = nullptr;
			}
			runtimeLibrary.api = nullptr;
			runtimeLibrary.runtimeBinaryPath.clear();
		}

		void UnloadAllRuntimeLibraries(GameModuleRegistryState& state) {
			for (auto& [gameName, runtimeLibrary] : state.loadedRuntimeLibraries) {
				(void)gameName;
				UnloadRuntimeLibrary(runtimeLibrary);
			}
			state.loadedRuntimeLibraries.clear();
		}

		[[nodiscard]] std::string NormalizeGameName(std::string_view value) {
			std::string normalized(value);
			std::transform(
				normalized.begin(),
				normalized.end(),
				normalized.begin(),
				[](unsigned char ch) { return static_cast<char>(std::tolower(ch)); }
			);
			return normalized;
		}

		[[nodiscard]] bool IsRepositoryRoot(
			const std::filesystem::path& path
		) {
			std::error_code ec;
			if (!std::filesystem::exists(path, ec) || ec) {
				return false;
			}

			return std::filesystem::exists(path / "premake5.lua", ec) && !ec &&
			       std::filesystem::exists(path / "src", ec) && !ec &&
			       std::filesystem::exists(path / "projects", ec) && !ec;
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryFindRepositoryRoot(
			std::filesystem::path startPath
		) {
			std::error_code ec;
			if (startPath.empty()) {
				return std::nullopt;
			}

			startPath = std::filesystem::weakly_canonical(startPath, ec);
			if (ec) {
				return std::nullopt;
			}

			if (!std::filesystem::is_directory(startPath, ec) || ec) {
				startPath = startPath.parent_path();
			}

			for (std::filesystem::path cursor = startPath;; cursor = cursor.parent_path()) {
				if (IsRepositoryRoot(cursor)) {
					return cursor;
				}
				if (!cursor.has_parent_path() || cursor == cursor.parent_path()) {
					break;
				}
			}

			return std::nullopt;
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryGetExecutableDirectory() {
			std::vector<wchar_t> buffer(260, L'\0');
			DWORD                copied = 0;
			while (true) {
				copied = ::GetModuleFileNameW(
					nullptr,
					buffer.data(),
					static_cast<DWORD>(buffer.size())
				);
				if (copied == 0) {
					return std::nullopt;
				}

				if (copied < buffer.size() - 1) {
					break;
				}
				buffer.resize(buffer.size() * 2, L'\0');
			}

			const std::filesystem::path exePath(
				std::wstring_view(buffer.data(), copied)
			);
			return exePath.parent_path();
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryGetEnvironmentRepoRoot() {
			constexpr wchar_t kEnvVarName[] = L"UNNAMED_REPO_ROOT";
			const DWORD       requiredChars =
				::GetEnvironmentVariableW(kEnvVarName, nullptr, 0);
			if (requiredChars == 0) {
				return std::nullopt;
			}

			std::vector<wchar_t> buffer(requiredChars, L'\0');
			const DWORD          writtenChars = ::GetEnvironmentVariableW(
				kEnvVarName,
				buffer.data(),
				requiredChars
			);
			if (writtenChars == 0 || writtenChars >= requiredChars) {
				return std::nullopt;
			}

			const std::filesystem::path envPath(
				std::wstring_view(buffer.data(), writtenChars)
			);
			return envPath;
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryGetEnvironmentProjectsRoot() {
			constexpr wchar_t kEnvVarName[] = L"UNNAMED_PROJECTS_ROOT";
			const DWORD       requiredChars =
				::GetEnvironmentVariableW(kEnvVarName, nullptr, 0);
			if (requiredChars == 0) {
				return std::nullopt;
			}

			std::vector<wchar_t> buffer(requiredChars, L'\0');
			const DWORD          writtenChars = ::GetEnvironmentVariableW(
				kEnvVarName,
				buffer.data(),
				requiredChars
			);
			if (writtenChars == 0 || writtenChars >= requiredChars) {
				return std::nullopt;
			}

			const std::filesystem::path envPath(
				std::wstring_view(buffer.data(), writtenChars)
			);
			return envPath;
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryResolveRepositoryRootFromExplicitPath(
			const std::filesystem::path& explicitPath
		) {
			std::error_code ec;
			const std::filesystem::path canonicalPath =
				std::filesystem::weakly_canonical(explicitPath, ec);
			if (ec) {
				return std::nullopt;
			}
			return TryFindRepositoryRoot(canonicalPath);
		}

		[[nodiscard]] std::optional<std::filesystem::path> TryResolveProjectsRootFromExplicitPath(
			const std::filesystem::path& explicitPath
		) {
			std::error_code ec;
			const std::filesystem::path canonicalPath =
				std::filesystem::weakly_canonical(explicitPath, ec);
			if (ec) {
				return std::nullopt;
			}

			if (std::filesystem::is_directory(canonicalPath, ec) && !ec) {
				if (canonicalPath.filename() == "projects") {
					return canonicalPath;
				}
				const std::filesystem::path projectsChild = canonicalPath / "projects";
				if (std::filesystem::is_directory(projectsChild, ec) && !ec) {
					return projectsChild;
				}
			}

			return std::nullopt;
		}

		struct RepositoryRootCandidate {
			std::filesystem::path root;
			std::string           reason;
		};

		[[nodiscard]] std::vector<RepositoryRootCandidate> BuildRepositoryRootCandidates(
			const ManifestSearchConfiguration& config
		) {
			std::vector<RepositoryRootCandidate> candidates;
			std::unordered_set<std::string>      dedupe;
			const auto addCandidate = [&](const std::filesystem::path& root, const std::string_view reason) {
				if (root.empty()) {
					return;
				}

				std::error_code ec;
				const std::filesystem::path normalized =
					std::filesystem::weakly_canonical(root, ec);
				const std::filesystem::path candidateRoot = ec ?
					std::filesystem::path(root).lexically_normal() :
					normalized;
				const std::string key = candidateRoot.generic_string();
				if (dedupe.insert(key).second) {
					candidates.push_back(RepositoryRootCandidate{
						.root = candidateRoot,
						.reason = std::string(reason),
					});
				}
			};

			if (config.explicitRepoRootOverride.has_value()) {
				if (const auto resolved = TryResolveRepositoryRootFromExplicitPath(
						*config.explicitRepoRootOverride
					); resolved.has_value()) {
					addCandidate(*resolved, "cli-repo-root");
				} else {
					DevMsg(
						kChannel,
						"ignored invalid --repo-root '{}'",
						config.explicitRepoRootOverride->generic_string()
					);
				}
			}

			if (const auto envRepoRoot = TryGetEnvironmentRepoRoot();
				envRepoRoot.has_value()) {
				if (const auto resolved = TryResolveRepositoryRootFromExplicitPath(
						*envRepoRoot
					); resolved.has_value()) {
					addCandidate(*resolved, "env:UNNAMED_REPO_ROOT");
				} else {
					DevMsg(
						kChannel,
						"ignored invalid UNNAMED_REPO_ROOT '{}'",
						envRepoRoot->generic_string()
					);
				}
			}

			const std::filesystem::path currentPath = std::filesystem::current_path();
			if (const auto resolved = TryFindRepositoryRoot(currentPath);
				resolved.has_value()) {
				addCandidate(*resolved, "cwd-upward");
			}

			if (const auto exeDirectory = TryGetExecutableDirectory();
				exeDirectory.has_value()) {
				if (const auto resolved = TryFindRepositoryRoot(*exeDirectory);
					resolved.has_value()) {
					addCandidate(*resolved, "exe-upward");
				}
			}

			return candidates;
		}

		[[nodiscard]] std::optional<RepositoryRootCandidate> ResolveRepositoryRootForManifestSearch(
			const ManifestSearchConfiguration& config
		) {
			const std::vector<RepositoryRootCandidate> candidates =
				BuildRepositoryRootCandidates(config);
			if (candidates.empty()) {
				return std::nullopt;
			}
			return candidates.front();
		}

		[[nodiscard]] bool RegisterProfile(
			GameModuleRegistryState& state,
			GameModulePaths          paths
		) {
			const std::string canonicalName = NormalizeGameName(paths.gameName);
			if (canonicalName.empty()) {
				return false;
			}

			if (state.modulesByName.contains(canonicalName)) {
				Error(
					kChannel,
					"duplicate gameName '{}' detected in manifest '{}'; keeping first registration.",
					paths.gameName,
					paths.resolvedManifestPath
				);
				return false;
			}

			// Game 固有情報はプロファイルとして保持し、生成関数は後から差し込めるようにする。
			RegisteredGameModule& entry = state.modulesByName[canonicalName];
			entry.paths = std::move(paths);
			state.aliasToCanonical.emplace(canonicalName, canonicalName);
			return true;
		}

		[[nodiscard]] std::filesystem::path ResolveManifestBaseRoot(
			const std::filesystem::path& manifestPath
		) {
			const std::filesystem::path normalized = manifestPath.lexically_normal();
			const std::filesystem::path configDir = normalized.parent_path();
			const std::filesystem::path gameDir = configDir.parent_path();
			const std::filesystem::path projectsDir = gameDir.parent_path();
			if (configDir.filename() == "config" &&
			    normalized.filename() == "game_profile.json" &&
			    projectsDir.filename() == "projects" && projectsDir.has_parent_path()) {
				return projectsDir.parent_path();
			}
			return configDir;
		}

		[[nodiscard]] std::string ResolvePathAgainstBaseRoot(
			const std::filesystem::path& baseRoot,
			const std::string&           value
		) {
			if (value.empty()) {
				return value;
			}

			const std::filesystem::path asPath(value);
			if (asPath.is_absolute()) {
				return asPath.lexically_normal().generic_string();
			}

			return (baseRoot / asPath).lexically_normal().generic_string();
		}

		[[nodiscard]] bool TryLoadJsonObjectFile(
			const std::filesystem::path& path,
			nlohmann::json&               outJson,
			std::string&                  outError
		) {
			std::ifstream input(path, std::ios::binary);
			if (!input.is_open()) {
				outError = "file not found";
				return false;
			}
			try {
				input >> outJson;
			} catch (const std::exception& ex) {
				outError = std::format("parse error: {}", ex.what());
				return false;
			}
			if (!outJson.is_object()) {
				outError = "invalid root type: expected object";
				return false;
			}
			return true;
		}

		[[nodiscard]] std::vector<std::string> ParseVersionParts(
			const std::string_view version
		) {
			std::vector<std::string> parts;
			std::string current;
			for (char ch : version) {
				if (ch == '.') {
					parts.emplace_back(current);
					current.clear();
					continue;
				}
				current.push_back(ch);
			}
			parts.emplace_back(current);
			return parts;
		}

		[[nodiscard]] bool TryParseVersionNumber(
			const std::string_view text,
			int&                   outValue
		) {
			if (text.empty()) {
				return false;
			}
			for (char ch : text) {
				if (ch < '0' || ch > '9') {
					return false;
				}
			}
			outValue = std::stoi(std::string(text));
			return true;
		}

		[[nodiscard]] int CompareVersions(
			const std::string_view lhsVersion,
			const std::string_view rhsVersion,
			bool&                  outComparable
		) {
			outComparable = false;
			const std::vector<std::string> lhsParts = ParseVersionParts(lhsVersion);
			const std::vector<std::string> rhsParts = ParseVersionParts(rhsVersion);
			const std::size_t partCount = std::max(lhsParts.size(), rhsParts.size());
			for (std::size_t i = 0; i < partCount; ++i) {
				const std::string_view lhsPart = i < lhsParts.size() ?
				                                     std::string_view(lhsParts[i]) :
				                                     std::string_view("0");
				const std::string_view rhsPart = i < rhsParts.size() ?
				                                     std::string_view(rhsParts[i]) :
				                                     std::string_view("0");
				int lhsValue = 0;
				int rhsValue = 0;
				if (!TryParseVersionNumber(lhsPart, lhsValue) ||
				    !TryParseVersionNumber(rhsPart, rhsValue)) {
					return 0;
				}
				if (lhsValue < rhsValue) {
					outComparable = true;
					return -1;
				}
				if (lhsValue > rhsValue) {
					outComparable = true;
					return 1;
				}
			}
			outComparable = true;
			return 0;
		}

		[[nodiscard]] bool SatisfiesVersionConstraint(
			const std::string_view version,
			const std::string_view constraint
		) {
			if (constraint.empty()) {
				return true;
			}

			std::string op = "==";
			std::string rhs = std::string(constraint);
			if (constraint.starts_with(">=") || constraint.starts_with("<=") ||
			    constraint.starts_with("==")) {
				op = std::string(constraint.substr(0, 2));
				rhs = std::string(constraint.substr(2));
			} else if (constraint.starts_with(">") || constraint.starts_with("<") ||
			           constraint.starts_with("=")) {
				op = std::string(constraint.substr(0, 1));
				rhs = std::string(constraint.substr(1));
			}

			bool comparable = false;
			const int cmp = CompareVersions(version, rhs, comparable);
			if (!comparable) {
				return version == rhs;
			}
			if (op == "==" || op == "=") {
				return cmp == 0;
			}
			if (op == ">") {
				return cmp > 0;
			}
			if (op == ">=") {
				return cmp >= 0;
			}
			if (op == "<") {
				return cmp < 0;
			}
			if (op == "<=") {
				return cmp <= 0;
			}
			return false;
		}

		[[nodiscard]] bool ContainsApiVersion(
			const std::span<const std::string_view> versions,
			const std::string_view                  target
		) {
			return std::ranges::find(versions, target) != versions.end();
		}

		[[nodiscard]] std::string FormatApiVersionSet(
			const std::span<const std::string_view> versions
		) {
			if (versions.empty()) {
				return "<none>";
			}

			std::string result;
			for (std::size_t i = 0; i < versions.size(); ++i) {
				if (i != 0) {
					result += ", ";
				}
				result += versions[i];
			}
			return result;
		}

		[[nodiscard]] ApiCompatibilityResult EvaluateApiCompatibility(
			const std::string_view                  apiLabel,
			const std::string_view                  requestedVersion,
			const std::span<const std::string_view> supportedVersions,
			const std::span<const std::string_view> deprecatedVersions,
			const std::string_view                  deprecationNote
		) {
			if (ContainsApiVersion(supportedVersions, requestedVersion)) {
				return {
					.level = ApiCompatibilityLevel::Supported,
					.message = std::format(
						"{} '{}' is supported.",
						apiLabel,
						requestedVersion
					),
				};
			}

			if (ContainsApiVersion(deprecatedVersions, requestedVersion)) {
				return {
					.level = ApiCompatibilityLevel::Deprecated,
					.message = std::format(
						"{} '{}' is deprecated. {}",
						apiLabel,
						requestedVersion,
						deprecationNote
					),
				};
			}

			return {
				.level = ApiCompatibilityLevel::Unsupported,
				.message = std::format(
					"{} '{}' is unsupported. supported=[{}] deprecated=[{}]",
					apiLabel,
					requestedVersion,
					FormatApiVersionSet(supportedVersions),
					FormatApiVersionSet(deprecatedVersions)
				),
			};
		}

		void ResolveProfileRootsAgainstBaseRoot(
			LoadedGameProfile&            profile,
			const std::filesystem::path& baseRoot
		) {
			profile.paths.gameRoot = ResolvePathAgainstBaseRoot(
				baseRoot,
				profile.paths.gameRoot
			);
			profile.paths.contentRoot = ResolvePathAgainstBaseRoot(
				baseRoot,
				profile.paths.contentRoot
			);
			profile.paths.configRoot = ResolvePathAgainstBaseRoot(
				baseRoot,
				profile.paths.configRoot
			);
			profile.paths.runtimeBinaryPath = ResolvePathAgainstBaseRoot(
				std::filesystem::path(profile.paths.gameRoot),
				profile.paths.runtimeBinaryPath
			);
			const std::filesystem::path gameRootPath(profile.paths.gameRoot);
			const auto resolveMountRoots = [&](std::vector<std::string>& mountRoots) {
				for (std::string& mountRoot : mountRoots) {
					mountRoot = ResolvePathAgainstBaseRoot(gameRootPath, mountRoot);
				}
			};
			resolveMountRoots(profile.paths.baseContentMountRoots);
			resolveMountRoots(profile.paths.dlcContentMountRoots);
			resolveMountRoots(profile.paths.modContentMountRoots);

			// 互換性のため、base マウント未指定時は contentRoot を既定 base として扱う。
			if (!profile.paths.contentRoot.empty()) {
				if (profile.paths.baseContentMountRoots.empty()) {
					profile.paths.baseContentMountRoots.emplace_back(
						profile.paths.contentRoot
					);
				} else if (std::ranges::find(
					               profile.paths.baseContentMountRoots,
					               profile.paths.contentRoot
				               ) ==
				           profile.paths.baseContentMountRoots.end()) {
					profile.paths.baseContentMountRoots.insert(
						profile.paths.baseContentMountRoots.begin(),
						profile.paths.contentRoot
					);
				}
			}
		}

		[[nodiscard]] bool TryReadRequiredStringField(
			const nlohmann::json& root,
			const std::string_view fieldName,
			const std::string_view manifestPath,
			std::string&           outValue
		) {
			const auto it = root.find(std::string(fieldName));
			if (it == root.end() || !it->is_string()) {
				DevMsg(
					kChannel,
					"manifest '{}' missing required string field '{}'",
					manifestPath,
					fieldName
				);
				return false;
			}

			outValue = it->get<std::string>();
			return true;
		}

		[[nodiscard]] bool TryReadRequiredIntegerField(
			const nlohmann::json& root,
			const std::string_view fieldName,
			const std::string_view manifestPath,
			int&                   outValue
		) {
			const auto it = root.find(std::string(fieldName));
			if (it == root.end() || !it->is_number_integer()) {
				DevMsg(
					kChannel,
					"manifest '{}' missing required integer field '{}'",
					manifestPath,
					fieldName
				);
				return false;
			}

			outValue = it->get<int>();
			return true;
		}

		[[nodiscard]] bool TryReadOptionalBoolField(
			const nlohmann::json& root,
			const std::string_view fieldName,
			const std::string_view manifestPath,
			bool&                 outValue
		) {
			const auto it = root.find(std::string(fieldName));
			if (it == root.end()) {
				return true;
			}
			if (!it->is_boolean()) {
				DevMsg(
					kChannel,
					"manifest '{}' field '{}' must be a boolean when provided",
					manifestPath,
					fieldName
				);
				return false;
			}
			outValue = it->get<bool>();
			return true;
		}

		[[nodiscard]] bool TryReadOptionalStringArrayField(
			const nlohmann::json& root,
			const std::string_view fieldName,
			const std::string_view manifestPath,
			std::vector<std::string>& outValues
		) {
			const auto it = root.find(std::string(fieldName));
			if (it == root.end()) {
				return true;
			}
			if (!it->is_array()) {
				DevMsg(
					kChannel,
					"manifest '{}' field '{}' must be a string array when provided",
					manifestPath,
					fieldName
				);
				return false;
			}
			for (const nlohmann::json& element : *it) {
				if (!element.is_string()) {
					DevMsg(
						kChannel,
						"manifest '{}' field '{}' contains non-string entry",
						manifestPath,
						fieldName
					);
					return false;
				}
				outValues.emplace_back(element.get<std::string>());
			}
			return true;
		}

		[[nodiscard]] ManifestLoadResult LoadGameProfileManifest(
			const std::string_view manifestPath
		) {
			ManifestLoadResult result = {};

			std::ifstream input(std::string(manifestPath), std::ios::binary);
			if (!input.is_open()) {
				result.failureReason = "file not found";
				return result;
			}

			nlohmann::json root = nlohmann::json::object();
			try {
				input >> root;
			} catch (const std::exception& ex) {
				result.failureReason = std::format("parse error: {}", ex.what());
				return result;
			}

			if (!root.is_object()) {
				result.failureReason = "invalid root type: expected object";
				return result;
			}

			int schemaVersion = 0;
			if (!TryReadRequiredIntegerField(
					root,
					"schemaVersion",
					manifestPath,
					schemaVersion
				)) {
				result.failureReason =
					"missing or invalid required integer field 'schemaVersion'";
				return result;
			}

			if (schemaVersion != 1) {
				result.failureReason = "unsupported schemaVersion " +
				                       std::to_string(schemaVersion) +
				                       " (supported: 1)";
				DevMsg(
					kChannel,
					"manifest '{}' has unsupported schemaVersion {} (supported: 1)",
					manifestPath,
					schemaVersion
				);
				return result;
			}

			LoadedGameProfile profile;
			if (!TryReadRequiredStringField(
					root,
					"gameName",
					manifestPath,
					profile.paths.gameName
				) ||
				!TryReadRequiredStringField(
					root,
					"gameRoot",
					manifestPath,
					profile.paths.gameRoot
				) ||
				!TryReadRequiredStringField(
					root,
					"contentRoot",
					manifestPath,
					profile.paths.contentRoot
				) ||
				!TryReadRequiredStringField(
					root,
					"configRoot",
					manifestPath,
					profile.paths.configRoot
				) ||
				!TryReadRequiredStringField(
					root,
					"defaultStartupScene",
					manifestPath,
					profile.paths.defaultStartupScene
				)) {
				result.failureReason = "missing required string field(s)";
				return result;
			}

			if (const auto runtimeBinaryIt = root.find("runtimeBinary");
				runtimeBinaryIt != root.end()) {
				if (!runtimeBinaryIt->is_string()) {
					DevMsg(
						kChannel,
						"manifest '{}' field 'runtimeBinary' must be a string when provided",
						manifestPath
					);
					result.failureReason =
						"invalid optional string field 'runtimeBinary'";
					return result;
				}
				profile.paths.runtimeBinaryPath =
					runtimeBinaryIt->get<std::string>();
			}
			if (!TryReadOptionalBoolField(
					root,
					"requireRuntimeBinary",
					manifestPath,
					profile.paths.requireRuntimeBinary
				) ||
				!TryReadOptionalBoolField(
					root,
					"preferRuntimeBinary",
					manifestPath,
					profile.paths.preferRuntimeBinary
				)) {
				result.failureReason =
					"invalid optional boolean field(s) for runtime policy";
				return result;
			}
			if (const auto mountsIt = root.find("mounts");
				mountsIt != root.end()) {
				if (!mountsIt->is_object()) {
					DevMsg(
						kChannel,
						"manifest '{}' field 'mounts' must be an object when provided",
						manifestPath
					);
					result.failureReason = "invalid optional object field 'mounts'";
					return result;
				}
				if (!TryReadOptionalStringArrayField(
						*mountsIt,
						"base",
						manifestPath,
						profile.paths.baseContentMountRoots
					) ||
					!TryReadOptionalStringArrayField(
						*mountsIt,
						"dlc",
						manifestPath,
						profile.paths.dlcContentMountRoots
					) ||
					!TryReadOptionalStringArrayField(
						*mountsIt,
						"mod",
						manifestPath,
						profile.paths.modContentMountRoots
					)) {
					result.failureReason =
						"invalid optional string array field(s) for mounts";
					return result;
				}
			}
			if (const auto modsIt = root.find("mods"); modsIt != root.end()) {
				if (!modsIt->is_object()) {
					DevMsg(
						kChannel,
						"manifest '{}' field 'mods' must be an object when provided",
						manifestPath
					);
					result.failureReason = "invalid optional object field 'mods'";
					return result;
				}

				if (const auto modsRootIt = modsIt->find("root");
					modsRootIt != modsIt->end()) {
					if (!modsRootIt->is_string()) {
						DevMsg(
							kChannel,
							"manifest '{}' field 'mods.root' must be a string when provided",
							manifestPath
						);
						result.failureReason =
							"invalid optional string field 'mods.root'";
						return result;
					}
					profile.modsRootOverride = modsRootIt->get<std::string>();
				}

				if (!TryReadOptionalStringArrayField(
						*modsIt,
						"enabled",
						manifestPath,
						profile.enabledMods
					) ||
					!TryReadOptionalStringArrayField(
						*modsIt,
						"disabled",
						manifestPath,
						profile.disabledMods
					)) {
					result.failureReason =
						"invalid optional string array field(s) for mods";
					return result;
				}
			}

			const auto aliasesIt = root.find("aliases");
			if (aliasesIt == root.end() || !aliasesIt->is_array()) {
				DevMsg(
					kChannel,
					"manifest '{}' missing required string array field 'aliases'",
					manifestPath
				);
				result.failureReason = "missing required string array field 'aliases'";
				return result;
			}

			for (const nlohmann::json& aliasNode : *aliasesIt) {
				if (!aliasNode.is_string()) {
					DevMsg(
						kChannel,
						"manifest '{}' contains non-string alias entry",
						manifestPath
					);
					result.failureReason = "aliases contains non-string entry";
					return result;
				}
				profile.aliases.emplace_back(aliasNode.get<std::string>());
			}

			DevMsg(
				kChannel,
				"loaded manifest '{}' for game '{}'",
				manifestPath,
				profile.paths.gameName
			);
			result.profile = std::move(profile);
			return result;
		}

		[[nodiscard]] bool TryReadModDependencyEntry(
			const nlohmann::json& dependencyNode,
			ModDependencySpec&    outDependency
		) {
			if (dependencyNode.is_string()) {
				outDependency.id = dependencyNode.get<std::string>();
				outDependency.versionConstraint.clear();
				return !outDependency.id.empty();
			}
			if (!dependencyNode.is_object()) {
				return false;
			}
			const auto idIt = dependencyNode.find("id");
			if (idIt == dependencyNode.end() || !idIt->is_string()) {
				return false;
			}
			outDependency.id = idIt->get<std::string>();
			if (const auto versionIt = dependencyNode.find("version");
				versionIt != dependencyNode.end()) {
				if (!versionIt->is_string()) {
					return false;
				}
				outDependency.versionConstraint = versionIt->get<std::string>();
			}
			return !outDependency.id.empty();
		}

		[[nodiscard]] std::optional<LoadedModManifest> TryLoadModManifest(
			const std::filesystem::path& manifestPath,
			std::string&                 outError
		) {
			nlohmann::json root = nlohmann::json::object();
			if (!TryLoadJsonObjectFile(manifestPath, root, outError)) {
				return std::nullopt;
			}

			int schemaVersion = 0;
			if (!root.contains("schemaVersion") ||
			    !root["schemaVersion"].is_number_integer()) {
				outError = "missing or invalid required integer field 'schemaVersion'";
				return std::nullopt;
			}
			schemaVersion = root["schemaVersion"].get<int>();
			if (schemaVersion != 1) {
				outError = std::format(
					"unsupported schemaVersion {} (supported: 1)",
					schemaVersion
				);
				return std::nullopt;
			}

			const auto requireStringField = [&](const std::string_view fieldName,
			                                    std::string&           outValue) {
				const auto it = root.find(std::string(fieldName));
				if (it == root.end() || !it->is_string()) {
					return false;
				}
				outValue = it->get<std::string>();
				return !outValue.empty();
			};

			LoadedModManifest manifest = {};
			if (!requireStringField("id", manifest.id) ||
			    !requireStringField("version", manifest.version) ||
			    !requireStringField("engineApi", manifest.engineApi) ||
			    !requireStringField("gameApi", manifest.gameApi)) {
				outError =
					"missing required string field(s): id/version/engineApi/gameApi";
				return std::nullopt;
			}
			if (const auto contentRootIt = root.find("contentRoot");
				contentRootIt != root.end()) {
				if (!contentRootIt->is_string()) {
					outError = "invalid optional string field 'contentRoot'";
					return std::nullopt;
				}
				manifest.contentRoot = contentRootIt->get<std::string>();
			}
			if (manifest.contentRoot.empty()) {
				manifest.contentRoot = "content";
			}

			if (const auto depsIt = root.find("deps"); depsIt != root.end()) {
				if (!depsIt->is_array()) {
					outError = "invalid optional array field 'deps'";
					return std::nullopt;
				}
				for (const nlohmann::json& depNode : *depsIt) {
					ModDependencySpec dep = {};
					if (!TryReadModDependencyEntry(depNode, dep)) {
						outError = "invalid dependency entry in 'deps'";
						return std::nullopt;
					}
					manifest.dependencies.emplace_back(std::move(dep));
				}
			}

			manifest.manifestPath = manifestPath;
			manifest.modRootPath = manifestPath.parent_path();
			return manifest;
		}

		void ResolveAndApplyModMountRoots(LoadedGameProfile& profile) {
			const std::filesystem::path gameRootPath(profile.paths.gameRoot);
			if (gameRootPath.empty()) {
				return;
			}

			std::filesystem::path modsRootPath = gameRootPath / "mods";
			if (!profile.modsRootOverride.empty()) {
				modsRootPath = std::filesystem::path(
					ResolvePathAgainstBaseRoot(gameRootPath, profile.modsRootOverride)
				);
			}
			std::error_code ec;
			if (!std::filesystem::exists(modsRootPath, ec) || ec) {
				DevMsg(
					kChannel,
					"mods root was not found for game '{}': '{}'",
					profile.paths.gameName,
					modsRootPath.generic_string()
				);
				return;
			}

			std::unordered_map<std::string, LoadedModManifest> manifestsById;
			for (std::filesystem::directory_iterator it(
				     modsRootPath,
				     std::filesystem::directory_options::skip_permission_denied,
				     ec
			     );
			     it != std::filesystem::directory_iterator();
			     it.increment(ec)) {
				if (ec) {
					ec.clear();
					continue;
				}
				if (!it->is_directory(ec) || ec) {
					ec.clear();
					continue;
				}

				const std::filesystem::path manifestPath =
					it->path() / "mod_manifest.json";
				if (!std::filesystem::exists(manifestPath, ec) || ec) {
					ec.clear();
					continue;
				}

				std::string parseError;
				const std::optional<LoadedModManifest> manifest = TryLoadModManifest(
					manifestPath,
					parseError
				);
				if (!manifest.has_value()) {
					Warning(
						kChannel,
						"mod manifest load failed '{}': {}",
						manifestPath.generic_string(),
						parseError
					);
					continue;
				}
				if (manifestsById.contains(manifest->id)) {
					Warning(
						kChannel,
						"duplicate mod id '{}' detected; keeping first manifest.",
						manifest->id
					);
					continue;
				}
				manifestsById.emplace(manifest->id, *manifest);
			}

			std::vector<std::string> enabledModIds;
			if (!profile.enabledMods.empty()) {
				enabledModIds = profile.enabledMods;
			} else {
				enabledModIds.reserve(manifestsById.size());
				for (const auto& [modId, manifest] : manifestsById) {
					(void)manifest;
					enabledModIds.emplace_back(modId);
				}
				std::ranges::sort(enabledModIds);
			}

			if (!profile.disabledMods.empty()) {
				std::unordered_set<std::string> disabledSet(
					profile.disabledMods.begin(),
					profile.disabledMods.end()
				);
				enabledModIds.erase(
					std::remove_if(
						enabledModIds.begin(),
						enabledModIds.end(),
						[&](const std::string& modId) {
							return disabledSet.contains(modId);
						}
					),
					enabledModIds.end()
				);
			}

			{
				std::unordered_set<std::string> seen;
				std::vector<std::string> uniqueEnabled;
				uniqueEnabled.reserve(enabledModIds.size());
				for (const std::string& modId : enabledModIds) {
					if (seen.emplace(modId).second) {
						uniqueEnabled.emplace_back(modId);
					}
				}
				enabledModIds = std::move(uniqueEnabled);
			}

			bool hasDependencyError = false;
			std::unordered_map<std::string, std::vector<std::string>> edgesByDependency;
			std::unordered_map<std::string, int> inDegree;
			Msg(
				kChannel,
				"api compatibility policy: engine supported=[{}] deprecated=[{}], game supported=[{}] deprecated=[{}]",
				FormatApiVersionSet(kSupportedEngineApiVersions),
				FormatApiVersionSet(kDeprecatedEngineApiVersions),
				FormatApiVersionSet(kSupportedGameApiVersions),
				FormatApiVersionSet(kDeprecatedGameApiVersions)
			);
			for (const std::string& modId : enabledModIds) {
				if (!manifestsById.contains(modId)) {
					Warning(
						kChannel,
						"enabled mod '{}' was not found under '{}'",
						modId,
						modsRootPath.generic_string()
					);
					hasDependencyError = true;
					continue;
				}

				const LoadedModManifest& modManifest = manifestsById.at(modId);
				const ApiCompatibilityResult engineApiCompatibility =
					EvaluateApiCompatibility(
						"engineApi",
						modManifest.engineApi,
						kSupportedEngineApiVersions,
						kDeprecatedEngineApiVersions,
						kEngineApiDeprecationNote
					);
				if (engineApiCompatibility.level ==
				    ApiCompatibilityLevel::Unsupported) {
					Warning(
						kChannel,
						"mod '{}' {}",
						modId,
						engineApiCompatibility.message
					);
					hasDependencyError = true;
				} else if (
					engineApiCompatibility.level ==
					ApiCompatibilityLevel::Deprecated) {
					Warning(
						kChannel,
						"mod '{}' {}",
						modId,
						engineApiCompatibility.message
					);
				}

				const ApiCompatibilityResult gameApiCompatibility =
					EvaluateApiCompatibility(
						"gameApi",
						modManifest.gameApi,
						kSupportedGameApiVersions,
						kDeprecatedGameApiVersions,
						kGameApiDeprecationNote
					);
				if (gameApiCompatibility.level ==
				    ApiCompatibilityLevel::Unsupported) {
					Warning(
						kChannel,
						"mod '{}' {}",
						modId,
						gameApiCompatibility.message
					);
					hasDependencyError = true;
				} else if (
					gameApiCompatibility.level ==
					ApiCompatibilityLevel::Deprecated) {
					Warning(
						kChannel,
						"mod '{}' {}",
						modId,
						gameApiCompatibility.message
					);
				}
				inDegree.emplace(modId, 0);
			}

			std::unordered_set<std::string> enabledSet(
				enabledModIds.begin(),
				enabledModIds.end()
			);
			for (const std::string& modId : enabledModIds) {
				if (!enabledSet.contains(modId) || !manifestsById.contains(modId)) {
					continue;
				}
				const LoadedModManifest& modManifest = manifestsById.at(modId);
				for (const ModDependencySpec& dependency : modManifest.dependencies) {
					if (!enabledSet.contains(dependency.id) ||
					    !manifestsById.contains(dependency.id)) {
						Warning(
							kChannel,
							"mod '{}' dependency '{}' is missing or disabled.",
							modId,
							dependency.id
						);
						hasDependencyError = true;
						continue;
					}
					const LoadedModManifest& dependencyManifest =
						manifestsById.at(dependency.id);
					if (!SatisfiesVersionConstraint(
						    dependencyManifest.version,
						    dependency.versionConstraint
					    )) {
						Warning(
							kChannel,
							"mod '{}' dependency version mismatch: dep='{}' required='{}' actual='{}'",
							modId,
							dependency.id,
							dependency.versionConstraint,
							dependencyManifest.version
						);
						hasDependencyError = true;
						continue;
					}

					edgesByDependency[dependency.id].emplace_back(modId);
					inDegree[modId]++;
				}
			}

			if (hasDependencyError) {
				Warning(
					kChannel,
					"mod loading aborted for game '{}': dependency/api validation failed.",
					profile.paths.gameName
				);
				profile.paths.modContentMountRoots.clear();
				return;
			}

			std::set<std::string> ready;
			for (const auto& [modId, degree] : inDegree) {
				if (degree == 0) {
					ready.emplace(modId);
				}
			}

			std::vector<std::string> orderedModIds;
			orderedModIds.reserve(enabledModIds.size());
			while (!ready.empty()) {
				const std::string modId = *ready.begin();
				ready.erase(ready.begin());
				orderedModIds.emplace_back(modId);
				if (!edgesByDependency.contains(modId)) {
					continue;
				}
				for (const std::string& dependentId : edgesByDependency.at(modId)) {
					inDegree[dependentId]--;
					if (inDegree[dependentId] == 0) {
						ready.emplace(dependentId);
					}
				}
			}

			if (orderedModIds.size() != inDegree.size()) {
				Warning(
					kChannel,
					"mod loading aborted for game '{}': circular dependency detected.",
					profile.paths.gameName
				);
				profile.paths.modContentMountRoots.clear();
				return;
			}

			profile.paths.modContentMountRoots.clear();
			for (const std::string& modId : orderedModIds) {
				const LoadedModManifest& manifest = manifestsById.at(modId);
				const std::string modContentRoot = ResolvePathAgainstBaseRoot(
					manifest.modRootPath,
					manifest.contentRoot
				);
				if (modContentRoot.empty()) {
					continue;
				}
				profile.paths.modContentMountRoots.emplace_back(modContentRoot);
			}

			Msg(
				kChannel,
				"resolved mods for game '{}': root='{}' enabled={} mounted={}",
				profile.paths.gameName,
				modsRootPath.generic_string(),
				enabledModIds.size(),
				profile.paths.modContentMountRoots.size()
			);
		}

		[[nodiscard]] bool RegisterAliasInternal(
			GameModuleRegistryState& state,
			std::string_view aliasName,
			std::string_view targetGameName
		) {
			const std::string alias = NormalizeGameName(aliasName);
			const std::string target = NormalizeGameName(targetGameName);
			if (alias.empty() || target.empty()) {
				return false;
			}

			if (!state.modulesByName.contains(target)) {
				return false;
			}

			if (const auto aliasIt = state.aliasToCanonical.find(alias);
				aliasIt != state.aliasToCanonical.end()) {
				return aliasIt->second == target;
			}

			state.aliasToCanonical[alias] = target;
			return true;
		}

		void RegisterAliases(
			GameModuleRegistryState&      state,
			const std::vector<std::string>& aliases,
			const std::string_view         canonicalName
		) {
			for (const std::string& alias : aliases) {
				if (!RegisterAliasInternal(state, alias, canonicalName)) {
					Error(
						kChannel,
						"alias conflict '{}' for game '{}'; keeping first registration.",
						alias,
						canonicalName
					);
				}
			}
		}

		void RegisterDefaultProfilesIfNeeded() {
			GameModuleRegistryState& state = GetRegistryState();
			if (state.defaultsRegistered) {
				return;
			}

			state.defaultsRegistered = true;

			const auto registerManifestAtPath =
				[&](
					const std::filesystem::path& manifestPath,
					const std::filesystem::path& baseRoot
				) {
					ManifestLoadResult loadResult = LoadGameProfileManifest(
						manifestPath.generic_string()
					);
					if (loadResult.profile.has_value()) {
						DevMsg(
							kChannel,
							"resolve manifest profile roots against baseRoot='{}'",
							baseRoot.generic_string()
						);
						ResolveProfileRootsAgainstBaseRoot(
							*loadResult.profile,
							baseRoot
						);
						loadResult.profile->paths.resolvedManifestPath =
							manifestPath.generic_string();
						ResolveAndApplyModMountRoots(*loadResult.profile);
						Msg(
							kChannel,
							"manifest profile loaded: game='{}' manifest='{}' gameRoot='{}' contentRoot='{}' configRoot='{}' defaultStartupScene='{}' runtimeBinary='{}' requireRuntimeBinary={} preferRuntimeBinary={} mounts(base={}, dlc={}, mod={})",
							loadResult.profile->paths.gameName,
							manifestPath.generic_string(),
							loadResult.profile->paths.gameRoot,
							loadResult.profile->paths.contentRoot,
							loadResult.profile->paths.configRoot,
							loadResult.profile->paths.defaultStartupScene,
							loadResult.profile->paths.runtimeBinaryPath,
							loadResult.profile->paths.requireRuntimeBinary,
							loadResult.profile->paths.preferRuntimeBinary,
							loadResult.profile->paths.baseContentMountRoots.size(),
							loadResult.profile->paths.dlcContentMountRoots.size(),
							loadResult.profile->paths.modContentMountRoots.size()
						);
					}

					const std::optional<LoadedGameProfile> profile =
						loadResult.profile;
					if (!profile.has_value()) {
						Warning(
							kChannel,
							"manifest load failed '{}' : {}",
							manifestPath.generic_string(),
							loadResult.failureReason
						);
						return;
					}

					const std::string canonicalName =
						NormalizeGameName(profile->paths.gameName);
					if (!RegisterProfile(state, profile->paths)) {
						return;
					}
					RegisterAliases(state, profile->aliases, canonicalName);
				};

			if (state.manifestSearch.explicitManifestPathOverride.has_value()) {
				std::error_code ec;
				std::filesystem::path manifestPath =
					std::filesystem::weakly_canonical(
						*state.manifestSearch.explicitManifestPathOverride,
						ec
					);
				if (ec) {
					manifestPath = state.manifestSearch.explicitManifestPathOverride
						               ->lexically_normal();
				}

				if (!std::filesystem::exists(manifestPath, ec) || ec) {
					Error(
						kChannel,
						"explicit manifest does not exist '{}'.",
						manifestPath.generic_string()
					);
					return;
				}
				if (!std::filesystem::is_regular_file(manifestPath, ec) || ec) {
					Error(
						kChannel,
						"explicit manifest is not a file '{}'.",
						manifestPath.generic_string()
					);
					return;
				}

				DevMsg(
					kChannel,
					"manifest discovery mode: explicit manifest '{}'",
					manifestPath.generic_string()
				);
				registerManifestAtPath(
					manifestPath,
					ResolveManifestBaseRoot(manifestPath)
				);
				return;
			}

			std::filesystem::path projectsRoot;
			std::filesystem::path manifestsBaseRoot;
			std::string           projectsRootReason;
			if (state.manifestSearch.explicitProjectsRootOverride.has_value()) {
				if (const auto resolvedProjectsRoot =
						TryResolveProjectsRootFromExplicitPath(
							*state.manifestSearch.explicitProjectsRootOverride
						); resolvedProjectsRoot.has_value()) {
					projectsRoot = *resolvedProjectsRoot;
					manifestsBaseRoot = projectsRoot.parent_path();
					projectsRootReason = "cli-projects-root";
				} else {
					Warning(
						kChannel,
						"ignored invalid --projects-root '{}' (expected '<...>/projects' or repository root containing 'projects').",
						state.manifestSearch.explicitProjectsRootOverride
							->generic_string()
					);
				}
			}

			if (projectsRoot.empty()) {
				if (const auto envProjectsRoot = TryGetEnvironmentProjectsRoot();
					envProjectsRoot.has_value()) {
					std::error_code ec;
					projectsRoot = std::filesystem::weakly_canonical(
						*envProjectsRoot,
						ec
					);
					if (ec) {
						projectsRoot = envProjectsRoot->lexically_normal();
					}
					manifestsBaseRoot = projectsRoot.parent_path();
					projectsRootReason = "env:UNNAMED_PROJECTS_ROOT";
				}
			}

			if (projectsRoot.empty()) {
				const std::optional<RepositoryRootCandidate> resolvedRepoRoot =
					ResolveRepositoryRootForManifestSearch(state.manifestSearch);
				if (!resolvedRepoRoot.has_value()) {
					Error(
						kChannel,
						"manifest discovery failed: repository root was not resolved."
					);
					return;
				}
				projectsRoot = resolvedRepoRoot->root / "projects";
				manifestsBaseRoot = resolvedRepoRoot->root;
				projectsRootReason =
					"repo-root:" + resolvedRepoRoot->reason;
			}

			std::error_code ec;
			if (!std::filesystem::exists(projectsRoot, ec) || ec) {
				Error(
					kChannel,
					"manifest discovery failed: projects root does not exist '{}'.",
					projectsRoot.generic_string()
				);
				return;
			}

			std::set<std::string> manifestPaths;
			for (std::filesystem::directory_iterator it(
				     projectsRoot,
				     std::filesystem::directory_options::skip_permission_denied,
				     ec
			     );
			     it != std::filesystem::directory_iterator();
			     it.increment(ec)) {
				if (ec) {
					ec.clear();
					continue;
				}
				if (!it->is_directory(ec) || ec) {
					ec.clear();
					continue;
				}

				const std::filesystem::path manifestPath =
					it->path() / "config" / "game_profile.json";
				if (!std::filesystem::exists(manifestPath, ec) || ec) {
					ec.clear();
					continue;
				}
				manifestPaths.emplace(manifestPath.lexically_normal().generic_string());
			}

			DevMsg(
				kChannel,
				"manifest discovery root: '{}' ({}) found={} manifests",
				projectsRoot.generic_string(),
				projectsRootReason,
				manifestPaths.size()
			);
			for (const std::string& manifestPath : manifestPaths) {
				registerManifestAtPath(
					std::filesystem::path(manifestPath),
					manifestsBaseRoot
				);
			}
		}

		[[nodiscard]] RuntimeBindingSnapshot TakeRuntimeBindingSnapshot(
			const GameModuleRegistryState& state
		) {
			RuntimeBindingSnapshot snapshot = {};
			for (const auto& [canonicalName, entry] : state.modulesByName) {
				if (entry.createFunction != nullptr) {
					snapshot.createFunctions.emplace(
						canonicalName,
						entry.createFunction
					);
				}
			}

			for (const auto& [aliasName, canonicalName] : state.aliasToCanonical) {
				if (aliasName == canonicalName) {
					continue;
				}
				if (!snapshot.createFunctions.contains(canonicalName)) {
					continue;
				}
				snapshot.aliases.emplace_back(aliasName, canonicalName);
			}

			return snapshot;
		}

		void RestoreRuntimeBindings(
			GameModuleRegistryState&          state,
			const RuntimeBindingSnapshot& snapshot
		) {
			for (const auto& [canonicalName, createFunction] : snapshot.createFunctions) {
				auto entryIt = state.modulesByName.find(canonicalName);
				if (entryIt == state.modulesByName.end()) {
					continue;
				}
				entryIt->second.createFunction = createFunction;
			}

			for (const auto& [aliasName, canonicalName] : snapshot.aliases) {
				(void)RegisterAliasInternal(state, aliasName, canonicalName);
			}
		}

		void ReloadProfilesPreserveRuntimeBindings() {
			GameModuleRegistryState& state = GetRegistryState();
			const RuntimeBindingSnapshot snapshot =
				TakeRuntimeBindingSnapshot(state);
			UnloadAllRuntimeLibraries(state);
			state.modulesByName.clear();
			state.aliasToCanonical.clear();
			state.defaultsRegistered = false;
			RegisterDefaultProfilesIfNeeded();
			RestoreRuntimeBindings(state, snapshot);
		}

		[[nodiscard]] std::string ResolveCanonicalName(
			const GameModuleRegistryState& state,
			std::string_view gameName
		) {
			const std::string normalized = NormalizeGameName(gameName);
			if (normalized.empty()) {
				return {};
			}

			const auto aliasIt = state.aliasToCanonical.find(normalized);
			if (aliasIt != state.aliasToCanonical.end()) {
				return aliasIt->second;
			}
			return normalized;
		}

		[[nodiscard]] const RegisteredGameModule* FindRegisteredGameModule(
			std::string_view gameName
		) {
			RegisterDefaultProfilesIfNeeded();
			const GameModuleRegistryState& state = GetRegistryState();
			const std::string canonicalName = ResolveCanonicalName(state, gameName);
			if (canonicalName.empty()) {
				return nullptr;
			}

			const auto entryIt = state.modulesByName.find(canonicalName);
			if (entryIt == state.modulesByName.end()) {
				return nullptr;
			}
			return &entryIt->second;
		}

		void CollectComponentTypesFromSceneJson(
			const nlohmann::json& root,
			std::set<std::string>& outTypes
		) {
			if (!root.is_object()) {
				return;
			}

			const auto entitiesIt = root.find("entities");
			if (entitiesIt == root.end() || !entitiesIt->is_array()) {
				return;
			}

			for (const nlohmann::json& entityNode : *entitiesIt) {
				if (!entityNode.is_object()) {
					continue;
				}

				const auto compsIt = entityNode.find("components");
				if (compsIt == entityNode.end() || !compsIt->is_array()) {
					continue;
				}

				for (const nlohmann::json& compNode : *compsIt) {
					if (!compNode.is_object()) {
						continue;
					}

					const auto typeIt = compNode.find("type");
					if (typeIt == compNode.end() || !typeIt->is_string()) {
						continue;
					}

					const std::string type = typeIt->get<std::string>();
					if (!type.empty()) {
						outTypes.emplace(type);
					}
				}
			}
		}

		[[nodiscard]] bool TryLoadJsonFile(
			const std::string_view path,
			nlohmann::json&        outJson,
			std::string&           outError
		) {
			std::ifstream input(std::string(path), std::ios::binary);
			if (!input.is_open()) {
				outError = "file not found";
				return false;
			}

			try {
				input >> outJson;
			} catch (const std::exception& ex) {
				outError = std::format("parse error: {}", ex.what());
				return false;
			}

			return true;
		}

		[[nodiscard]] std::string DescribeRuntimeLoadFailure(
			const RuntimeLoadResult& result
		) {
			switch (result.failure) {
				case RuntimeLoadFailure::None:
					return {};
				case RuntimeLoadFailure::RuntimePathEmpty:
					return "runtimeBinary path is empty";
				case RuntimeLoadFailure::RuntimeBinaryNotFound:
					return std::format("runtime binary not found '{}'", result.runtimePath);
				case RuntimeLoadFailure::RuntimeBinaryNotFile:
					return std::format(
						"runtime binary is not a file '{}'",
						result.runtimePath
					);
				case RuntimeLoadFailure::LoadLibraryFailed:
					return std::format(
						"LoadLibraryW failed for '{}' (error={})",
						result.runtimePath,
						result.systemErrorCode
					);
				case RuntimeLoadFailure::MissingRuntimeSymbol:
					return std::format(
						"runtime '{}' is missing symbol '{}' (error={})",
						result.runtimePath,
						kGameRuntimeApiV1SymbolName,
						result.systemErrorCode
					);
				case RuntimeLoadFailure::InvalidRuntimeApi:
					return std::format(
						"runtime '{}' returned invalid GameRuntimeApiV1 (expectedVersion={}, gotVersion={}, structSize={})",
						result.runtimePath,
						static_cast<std::uint32_t>(GameRuntimeAbiVersion::Current),
						result.apiVersion,
						result.apiStructSize
					);
			}

			return "unknown runtime load failure";
		}

		class ImportedRuntimeGameModule final : public IGameModule {
		public:
			ImportedRuntimeGameModule(
				IGameModule* rawModule,
				void (*destroyFunction)(IGameModule*)
			)
				: mRawModule(rawModule)
				, mDestroyFunction(destroyFunction) {
			}

			~ImportedRuntimeGameModule() override {
				if (mRawModule != nullptr && mDestroyFunction != nullptr) {
					mDestroyFunction(mRawModule);
				}
				mRawModule = nullptr;
			}

			void Initialize(EngineServices& services) override {
				mRawModule->Initialize(services);
			}

			[[nodiscard]] std::unique_ptr<World> CreateRuntimeWorld(
				const WorldServices& services
			) override {
				return mRawModule->CreateRuntimeWorld(services);
			}

			[[nodiscard]] std::unique_ptr<World> CreatePlayWorld(
				const WorldServices& services
			) override {
				return mRawModule->CreatePlayWorld(services);
			}

			[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() override {
				return mRawModule->CreateDemoService();
			}

			void RegisterGameComponents(ComponentRegistry& componentRegistry) override {
				mRawModule->RegisterGameComponents(componentRegistry);
			}

			[[nodiscard]] GameModulePaths GetGameModulePaths() const override {
				return mRawModule->GetGameModulePaths();
			}

			[[nodiscard]] std::string GetDefaultStartupScenePath() const override {
				return mRawModule->GetDefaultStartupScenePath();
			}

			[[nodiscard]] std::string GetDefaultUiDocumentPath() const override {
				return mRawModule->GetDefaultUiDocumentPath();
			}

		private:
			IGameModule* mRawModule = nullptr;
			void (*mDestroyFunction)(IGameModule*) = nullptr;
		};

		[[nodiscard]] RuntimeLoadResult EnsureRuntimeLibraryLoaded(
			GameModuleRegistryState& state,
			const std::string_view   canonicalName,
			const GameModulePaths&   paths
		) {
			RuntimeLoadResult result = {};
			const std::string canonical(canonicalName);
			if (paths.runtimeBinaryPath.empty()) {
				result.failure = RuntimeLoadFailure::RuntimePathEmpty;
				return result;
			}

			std::error_code ec;
			const std::filesystem::path runtimePath =
				std::filesystem::path(paths.runtimeBinaryPath).lexically_normal();
			result.runtimePath = runtimePath.generic_string();
			if (auto runtimeIt = state.loadedRuntimeLibraries.find(canonical);
				runtimeIt != state.loadedRuntimeLibraries.end()) {
				const std::filesystem::path loadedRuntimePath =
					std::filesystem::path(runtimeIt->second.runtimeBinaryPath)
						.lexically_normal();
				if (loadedRuntimePath == runtimePath) {
					result.runtimeLibrary = &runtimeIt->second;
					return result;
				}

				Msg(
					kChannel,
					"runtime binary path changed for game '{}': old='{}' new='{}' (reload)",
					paths.gameName,
					runtimeIt->second.runtimeBinaryPath,
					runtimePath.generic_string()
				);
				UnloadRuntimeLibrary(runtimeIt->second);
				state.loadedRuntimeLibraries.erase(runtimeIt);
			}

			if (!std::filesystem::exists(runtimePath, ec) || ec) {
				result.failure = RuntimeLoadFailure::RuntimeBinaryNotFound;
				return result;
			}
			if (!std::filesystem::is_regular_file(runtimePath, ec) || ec) {
				result.failure = RuntimeLoadFailure::RuntimeBinaryNotFile;
				return result;
			}

			const std::wstring runtimePathWide = runtimePath.wstring();
			HMODULE            runtimeModule = ::LoadLibraryW(runtimePathWide.c_str());
			if (runtimeModule == nullptr) {
				result.failure = RuntimeLoadFailure::LoadLibraryFailed;
				result.systemErrorCode = ::GetLastError();
				return result;
			}

			FARPROC symbol = ::GetProcAddress(runtimeModule, kGameRuntimeApiV1SymbolName);
			if (symbol == nullptr) {
				result.failure = RuntimeLoadFailure::MissingRuntimeSymbol;
				result.systemErrorCode = ::GetLastError();
				::FreeLibrary(runtimeModule);
				return result;
			}

			const auto getApi =
				reinterpret_cast<GetGameRuntimeApiV1Function>(symbol);
			const GameRuntimeApiV1* api = getApi();
			if (!IsValidGameRuntimeApiV1(api)) {
				result.failure = RuntimeLoadFailure::InvalidRuntimeApi;
				result.apiVersion = api == nullptr ? 0u : api->abiVersion;
				result.apiStructSize = api == nullptr ? 0u : api->structSize;
				::FreeLibrary(runtimeModule);
				return result;
			}

			const char* runtimeName = api->GetRuntimeName();
			Msg(
				kChannel,
				"runtime library loaded: game='{}' runtime='{}' apiVersion={} name='{}'",
				paths.gameName,
				runtimePath.generic_string(),
				api->abiVersion,
				runtimeName == nullptr ? "" : runtimeName
			);

			LoadedRuntimeLibrary loadedRuntime = {};
			loadedRuntime.moduleHandle = runtimeModule;
			loadedRuntime.api = api;
			loadedRuntime.runtimeBinaryPath = runtimePath.generic_string();
			const auto [insertedIt, inserted] = state.loadedRuntimeLibraries.emplace(
				canonical,
				std::move(loadedRuntime)
			);
			(void)inserted;
			result.runtimeLibrary = &insertedIt->second;
			return result;
		}

		class DefaultGameModule final : public IGameModule {
		public:
			explicit DefaultGameModule(GameModulePaths paths)
				: mPaths(std::move(paths)) {
			}

			void Initialize(EngineServices& services) override {
				(void)services;
			}

			[[nodiscard]] std::unique_ptr<World> CreateRuntimeWorld(
				const WorldServices& services
			) override {
				auto world = std::make_unique<World>();
				world->SetServices(services);
				return world;
			}

			[[nodiscard]] std::unique_ptr<World> CreatePlayWorld(
				const WorldServices& services
			) override {
				auto world = std::make_unique<World>();
				world->SetServices(services);
				return world;
			}

			[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() override {
				return nullptr;
			}

			void RegisterGameComponents(ComponentRegistry& componentRegistry) override {
				(void)componentRegistry;
			}

			[[nodiscard]] GameModulePaths GetGameModulePaths() const override {
				return mPaths;
			}

			[[nodiscard]] std::string GetDefaultUiDocumentPath() const override {
				return {};
			}

		private:
			GameModulePaths mPaths;
		};

		class ProfileBoundGameModule final : public IGameModule {
		public:
			ProfileBoundGameModule(
				std::unique_ptr<IGameModule> innerModule,
				GameModulePaths              profilePaths
			)
				: mInnerModule(std::move(innerModule))
				, mProfilePaths(std::move(profilePaths)) {
			}

			void Initialize(EngineServices& services) override {
				mInnerModule->Initialize(services);
			}

			[[nodiscard]] std::unique_ptr<World> CreateRuntimeWorld(
				const WorldServices& services
			) override {
				return mInnerModule->CreateRuntimeWorld(services);
			}

			[[nodiscard]] std::unique_ptr<World> CreatePlayWorld(
				const WorldServices& services
			) override {
				return mInnerModule->CreatePlayWorld(services);
			}

			[[nodiscard]] std::unique_ptr<IDemoService> CreateDemoService() override {
				return mInnerModule->CreateDemoService();
			}

			void RegisterGameComponents(ComponentRegistry& componentRegistry) override {
				mInnerModule->RegisterGameComponents(componentRegistry);
			}

			[[nodiscard]] GameModulePaths GetGameModulePaths() const override {
				return mProfilePaths;
			}

			[[nodiscard]] std::string GetDefaultStartupScenePath() const override {
				return mProfilePaths.defaultStartupScene;
			}

			[[nodiscard]] std::string GetDefaultUiDocumentPath() const override {
				return mInnerModule->GetDefaultUiDocumentPath();
			}

		private:
			std::unique_ptr<IGameModule> mInnerModule;
			GameModulePaths              mProfilePaths;
		};
	}

	void RegisterDefaultGameModuleProfiles() {
		RegisterDefaultProfilesIfNeeded();
	}

	void SetGameModuleManifestRepoRootOverride(
		const std::filesystem::path& repoRootPath
	) {
		GameModuleRegistryState& state = GetRegistryState();
		if (repoRootPath.empty()) {
			DevMsg(kChannel, "ignored empty --repo-root override");
			return;
		}

		state.manifestSearch.explicitRepoRootOverride = repoRootPath;
		DevMsg(
			kChannel,
			"configured --repo-root override: '{}'",
			repoRootPath.generic_string()
		);
		if (state.defaultsRegistered) {
			DevMsg(
				kChannel,
				"--repo-root override was set after profile registration; reloading profiles with the updated root"
			);
			ReloadProfilesPreserveRuntimeBindings();
		}
	}

	void SetGameModuleManifestProjectsRootOverride(
		const std::filesystem::path& projectsRootPath
	) {
		GameModuleRegistryState& state = GetRegistryState();
		if (projectsRootPath.empty()) {
			DevMsg(kChannel, "ignored empty --projects-root override");
			return;
		}

		state.manifestSearch.explicitProjectsRootOverride = projectsRootPath;
		DevMsg(
			kChannel,
			"configured --projects-root override: '{}'",
			projectsRootPath.generic_string()
		);
		if (state.defaultsRegistered) {
			DevMsg(
				kChannel,
				"--projects-root override was set after profile registration; reloading profiles with the updated root"
			);
			ReloadProfilesPreserveRuntimeBindings();
		}
	}

	void SetGameModuleManifestPathOverride(
		const std::filesystem::path& manifestPath
	) {
		GameModuleRegistryState& state = GetRegistryState();
		if (manifestPath.empty()) {
			DevMsg(kChannel, "ignored empty --project override");
			return;
		}

		state.manifestSearch.explicitManifestPathOverride = manifestPath;
		DevMsg(
			kChannel,
			"configured --project override: '{}'",
			manifestPath.generic_string()
		);
		if (state.defaultsRegistered) {
			DevMsg(
				kChannel,
				"--project override was set after profile registration; reloading profiles with explicit manifest"
			);
			ReloadProfilesPreserveRuntimeBindings();
		}
	}

	std::string ResolveSingleRegisteredGameName() {
		RegisterDefaultProfilesIfNeeded();
		const GameModuleRegistryState& state = GetRegistryState();
		if (state.modulesByName.size() != 1) {
			return {};
		}

		const auto it = state.modulesByName.begin();
		if (it == state.modulesByName.end()) {
			return {};
		}

		if (it->second.paths.has_value()) {
			return it->second.paths->gameName;
		}
		return it->first;
	}

	bool PinGameModuleManifestToGame(const std::string_view gameName) {
		RegisterDefaultProfilesIfNeeded();
		GameModuleRegistryState& state = GetRegistryState();
		const std::string canonicalName = ResolveCanonicalName(state, gameName);
		if (canonicalName.empty()) {
			DevMsg(
				kChannel,
				"PinGameModuleManifestToGame('{}') failed: unresolved game name",
				gameName
			);
			return false;
		}

		const auto entryIt = state.modulesByName.find(canonicalName);
		if (entryIt == state.modulesByName.end() || !entryIt->second.paths.has_value()) {
			DevMsg(
				kChannel,
				"PinGameModuleManifestToGame('{}') failed: profile paths are not registered",
				gameName
			);
			return false;
		}

		const std::string& manifestPath =
			entryIt->second.paths->resolvedManifestPath;
		if (manifestPath.empty()) {
			DevMsg(
				kChannel,
				"PinGameModuleManifestToGame('{}') failed: resolvedManifestPath is empty",
				gameName
			);
			return false;
		}

		SetGameModuleManifestPathOverride(std::filesystem::path(manifestPath));
		return true;
	}

	bool RegisterGameModule(
		const std::string_view gameName,
		const GameModuleCreateFunction createFunction
	) {
		RegisterDefaultProfilesIfNeeded();
		if (createFunction == nullptr) {
			return false;
		}

		const std::string canonicalName = NormalizeGameName(gameName);
		if (canonicalName.empty()) {
			return false;
		}

		GameModuleRegistryState& state = GetRegistryState();
		if (!state.modulesByName.contains(canonicalName)) {
			DevMsg(
				kChannel,
				"cannot register runtime module for '{}': profile is not loaded from manifest",
				gameName
			);
			return false;
		}

		RegisteredGameModule& entry = state.modulesByName[canonicalName];
		entry.createFunction = createFunction;
		if (auto runtimeIt = state.loadedRuntimeLibraries.find(canonicalName);
			runtimeIt != state.loadedRuntimeLibraries.end()) {
			UnloadRuntimeLibrary(runtimeIt->second);
			state.loadedRuntimeLibraries.erase(runtimeIt);
		}
		state.aliasToCanonical[canonicalName] = canonicalName;
		return true;
	}

	bool RegisterGameModuleAlias(
		const std::string_view aliasName,
		const std::string_view targetGameName
	) {
		RegisterDefaultProfilesIfNeeded();
		GameModuleRegistryState& state = GetRegistryState();
		const std::string canonicalTarget = ResolveCanonicalName(state, targetGameName);
		if (canonicalTarget.empty()) {
			return false;
		}

		return RegisterAliasInternal(state, aliasName, canonicalTarget);
	}

	GameModulePaths ResolveGameModulePaths(const std::string_view gameName) {
		if (const RegisteredGameModule* entry = FindRegisteredGameModule(gameName);
			entry != nullptr && entry->paths.has_value()) {
			return *entry->paths;
		}

		DevMsg(
			kChannel,
			"no profile registered for '{}'; returning empty GameModulePaths",
			gameName
		);
		return {};
	}

	std::unique_ptr<IGameModule> CreateGameModule(const std::string_view gameName) {
		RegisterDefaultProfilesIfNeeded();
		GameModuleRegistryState& state = GetRegistryState();
		const std::string canonicalName = ResolveCanonicalName(state, gameName);
		if (canonicalName.empty()) {
			DevMsg(
				kChannel,
				"CreateGameModule('{}') failed: canonical game name is empty",
				gameName
			);
			return nullptr;
		}

		auto entryIt = state.modulesByName.find(canonicalName);
		if (entryIt == state.modulesByName.end()) {
			DevMsg(
				kChannel,
				"CreateGameModule('{}') failed: game profile is not registered",
				gameName
			);
			return nullptr;
		}

		RegisteredGameModule& entry = entryIt->second;
		const bool hasRuntimePaths = entry.paths.has_value();
		const bool hasRuntimeBinary = hasRuntimePaths &&
		                              !entry.paths->runtimeBinaryPath.empty();
		const bool requireRuntimeBinary = hasRuntimePaths &&
		                                  entry.paths->requireRuntimeBinary;
		const bool preferRuntimeBinary = hasRuntimePaths &&
		                                 entry.paths->preferRuntimeBinary;

		if (requireRuntimeBinary && !hasRuntimeBinary) {
			Error(
				kChannel,
				"CreateGameModule('{}') failed: runtimeBinary is required but not configured in manifest.",
				gameName
			);
			return nullptr;
		}

		const bool shouldTryDynamicRuntime =
			hasRuntimeBinary && (preferRuntimeBinary || entry.createFunction == nullptr);
		if (shouldTryDynamicRuntime) {
			const RuntimeLoadResult runtimeLoad = EnsureRuntimeLibraryLoaded(
				state,
				canonicalName,
				*entry.paths
			);
			LoadedRuntimeLibrary* runtimeLibrary = runtimeLoad.runtimeLibrary;
			if (runtimeLibrary == nullptr || runtimeLibrary->api == nullptr) {
				const std::string failureReason = DescribeRuntimeLoadFailure(runtimeLoad);
				if (requireRuntimeBinary || entry.createFunction == nullptr) {
					Error(
						kChannel,
						"CreateGameModule('{}') failed: runtime binary could not be loaded ({})",
						gameName,
						failureReason
					);
					return nullptr;
				}
				Warning(
					kChannel,
					"runtime binary load failed for '{}': {}. fallback to static runtime registration.",
					gameName,
					failureReason
				);
			} else {
				IGameModule* rawModule = runtimeLibrary->api->CreateGameModule();
				if (rawModule == nullptr) {
					if (requireRuntimeBinary || entry.createFunction == nullptr) {
						Error(
							kChannel,
							"CreateGameModule('{}') failed: runtime '{}' returned null module",
							gameName,
							runtimeLibrary->runtimeBinaryPath
						);
						return nullptr;
					}
					Warning(
						kChannel,
						"runtime '{}' returned null module for '{}', fallback to static runtime registration.",
						runtimeLibrary->runtimeBinaryPath,
						gameName
					);
				} else {
					std::unique_ptr<IGameModule> importedModule =
						std::make_unique<ImportedRuntimeGameModule>(
							rawModule,
							runtimeLibrary->api->DestroyGameModule
						);
					return std::make_unique<ProfileBoundGameModule>(
						std::move(importedModule),
						*entry.paths
					);
				}
			}
		}

		if (entry.createFunction != nullptr) {
			std::unique_ptr<IGameModule> module = entry.createFunction();
			if (!module) {
				DevMsg(
					kChannel,
					"CreateGameModule('{}') failed: registered createFunction returned null",
					gameName
				);
				return nullptr;
			}

			if (entry.paths.has_value()) {
				return std::make_unique<ProfileBoundGameModule>(
					std::move(module),
					*entry.paths
				);
			}
			return module;
		}

		// Editor など未リンク App では、Paths だけ持つ既定モジュールで起動する。
		if (entry.paths.has_value()) {
			return std::make_unique<DefaultGameModule>(*entry.paths);
		}

		DevMsg(
			kChannel,
			"CreateGameModule('{}') failed: profile exists but paths are missing",
			gameName
		);
		return nullptr;
	}

	bool ValidateGameModuleStartupProfile(
		IGameModule&                      gameModule,
		const StartupValidationOptions& options
	) {
		const GameModulePaths paths = gameModule.GetGameModulePaths();
		std::string startupSceneRelative = gameModule.GetDefaultStartupScenePath();
		if (startupSceneRelative.empty()) {
			startupSceneRelative = paths.defaultStartupScene;
		}
		const MountedContentResolution startupSceneResolution =
			ResolveGameMountedContentPathDetailed(paths, startupSceneRelative);
		const std::string startupScenePath = startupSceneResolution.resolvedPath;
		const std::string contentRoot = ResolveGameContentPath(paths, "");
		const std::string configRoot = ResolveGameConfigPath(paths, "");

		bool validationFailed = false;
		const auto reportIssue = [&](const bool isError, const std::string& text) {
			if (isError) {
				Error(kChannel, "startup validation: {}", text);
				validationFailed = true;
				return;
			}
			Warning(kChannel, "startup validation: {}", text);
		};

		if (options.emitDetailedLogs && !startupSceneResolution.resolvedRoot.empty()) {
			Msg(
				kChannel,
				"startup scene mount resolution: relative='{}' resolved='{}' layer='{}' root='{}' exists={}",
				startupSceneRelative,
				startupSceneResolution.resolvedPath,
				startupSceneResolution.resolvedLayer,
				startupSceneResolution.resolvedRoot,
				startupSceneResolution.existsOnDisk
			);
		}

		std::error_code ec;
		if (paths.resolvedManifestPath.empty()) {
			reportIssue(true, "resolved manifest path is empty.");
		} else if (
			!std::filesystem::exists(paths.resolvedManifestPath, ec) || ec
		) {
			reportIssue(
				true,
				std::format(
					"manifest does not exist '{}'.",
					paths.resolvedManifestPath
				)
			);
		}

		ec.clear();
		if (contentRoot.empty()) {
			reportIssue(true, "content root is empty.");
		} else if (!std::filesystem::exists(contentRoot, ec) || ec) {
			reportIssue(
				true,
				std::format("content root does not exist '{}'.", contentRoot)
			);
		}

		ec.clear();
		if (configRoot.empty()) {
			reportIssue(true, "config root is empty.");
		} else if (!std::filesystem::exists(configRoot, ec) || ec) {
			reportIssue(
				true,
				std::format("config root does not exist '{}'.", configRoot)
			);
		}

		ec.clear();
		if (startupScenePath.empty()) {
			reportIssue(true, "startup scene path is empty.");
		} else if (!std::filesystem::exists(startupScenePath, ec) || ec) {
			reportIssue(
				true,
				std::format("startup scene does not exist '{}'.", startupScenePath)
			);
		}

		ec.clear();
		if (paths.requireRuntimeBinary && paths.runtimeBinaryPath.empty()) {
			reportIssue(
				true,
				"runtime binary is required but runtimeBinary path is empty."
			);
		} else if (!paths.runtimeBinaryPath.empty()) {
			if (!std::filesystem::exists(paths.runtimeBinaryPath, ec) || ec) {
				reportIssue(
					true,
					std::format(
						"runtime binary does not exist '{}'.",
						paths.runtimeBinaryPath
					)
				);
			} else if (!std::filesystem::is_regular_file(paths.runtimeBinaryPath, ec) ||
			           ec) {
				reportIssue(
					true,
					std::format(
						"runtime binary is not a file '{}'.",
						paths.runtimeBinaryPath
					)
				);
			} else {
				RegisterDefaultProfilesIfNeeded();
				GameModuleRegistryState& state = GetRegistryState();
				std::string canonicalName = ResolveCanonicalName(
					state,
					paths.gameName
				);
				if (canonicalName.empty()) {
					canonicalName = NormalizeGameName(paths.gameName);
				}
				const RuntimeLoadResult runtimeLoad = EnsureRuntimeLibraryLoaded(
					state,
					canonicalName,
					paths
				);
				LoadedRuntimeLibrary* runtimeLibrary = runtimeLoad.runtimeLibrary;
				if (runtimeLibrary == nullptr || runtimeLibrary->api == nullptr) {
					const bool strictRuntimePolicy =
						paths.requireRuntimeBinary || paths.preferRuntimeBinary;
					const std::string failureReason = DescribeRuntimeLoadFailure(
						runtimeLoad
					);
					reportIssue(
						strictRuntimePolicy,
						std::format(
							"runtime binary failed to load API from '{}': {}.",
							paths.runtimeBinaryPath,
							failureReason
						)
					);
				}
			}
		}

		if (validationFailed) {
			return false;
		}

		ComponentRegistry& componentRegistry = ComponentRegistry::Get();
		RegisterDefaultEngineComponents(componentRegistry);
		gameModule.RegisterGameComponents(componentRegistry);

		nlohmann::json sceneJson = nlohmann::json::object();
		std::string sceneLoadError;
		if (!TryLoadJsonFile(startupScenePath, sceneJson, sceneLoadError)) {
			reportIssue(
				true,
				std::format(
					"startup scene parse failed '{}' ({})",
					startupScenePath,
					sceneLoadError
				)
			);
			return false;
		}

		std::set<std::string> sceneComponentTypes;
		CollectComponentTypesFromSceneJson(sceneJson, sceneComponentTypes);

		std::vector<std::string> unknownComponentTypes;
		for (const std::string& type : sceneComponentTypes) {
			if (!componentRegistry.IsRegistered(type)) {
				unknownComponentTypes.push_back(type);
			}
		}

		if (!unknownComponentTypes.empty()) {
			const std::string joined = std::accumulate(
				std::next(unknownComponentTypes.begin()),
				unknownComponentTypes.end(),
				unknownComponentTypes.front(),
				[](const std::string& lhs, const std::string& rhs) {
					return lhs + ", " + rhs;
				}
			);
			const bool failUnknown = options.failOnUnknownComponentTypes;
			reportIssue(
				failUnknown,
				std::format(
					"startup scene has unknown component types [{}].",
					joined
				)
			);
		}

		if (options.emitDetailedLogs) {
			Msg(
				kChannel,
				"startup validation completed: game='{}' manifest='{}' startupScene='{}' status={}",
				paths.gameName,
				paths.resolvedManifestPath,
				startupScenePath,
				validationFailed ? "failed" : "passed"
			);
		}

		return !validationFailed;
	}

	bool ValidateGameModuleStartupProfile(
		const std::string_view           gameName,
		const StartupValidationOptions& options
	) {
		std::unique_ptr<IGameModule> gameModule = CreateGameModule(gameName);
		if (!gameModule) {
			Error(
				kChannel,
				"startup validation: failed to create game module '{}'.",
				gameName
			);
			return false;
		}
		return ValidateGameModuleStartupProfile(*gameModule, options);
	}
}
