#include "GameModuleFactory.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <filesystem>
#include <fstream>
#include <map>
#include <numeric>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

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

		struct RegisteredGameModule {
			std::optional<GameModulePaths> paths;
			GameModuleCreateFunction createFunction = nullptr;
		};

		struct LoadedGameProfile {
			GameModulePaths            paths;
			std::vector<std::string> aliases;
		};

		struct ManifestLoadResult {
			std::optional<LoadedGameProfile> profile;
			std::string                      failureReason;
		};

		struct ManifestSearchConfiguration {
			std::optional<std::filesystem::path> explicitRepoRootOverride;
			std::optional<std::filesystem::path> explicitManifestPathOverride;
		};

		struct GameModuleRegistryState {
			std::unordered_map<std::string, RegisteredGameModule> modulesByName;
			std::unordered_map<std::string, std::string> aliasToCanonical;
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
						Msg(
							kChannel,
							"manifest profile loaded: game='{}' manifest='{}' gameRoot='{}' contentRoot='{}' configRoot='{}' defaultStartupScene='{}'",
							loadResult.profile->paths.gameName,
							manifestPath.generic_string(),
							loadResult.profile->paths.gameRoot,
							loadResult.profile->paths.contentRoot,
							loadResult.profile->paths.configRoot,
							loadResult.profile->paths.defaultStartupScene
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
			} else {
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
		const RegisteredGameModule* entry = FindRegisteredGameModule(gameName);
		if (entry == nullptr) {
			DevMsg(
				kChannel,
				"CreateGameModule('{}') failed: game profile is not registered",
				gameName
			);
			return nullptr;
		}

		if (entry->createFunction != nullptr) {
			std::unique_ptr<IGameModule> module = entry->createFunction();
			if (!module) {
				DevMsg(
					kChannel,
					"CreateGameModule('{}') failed: registered createFunction returned null",
					gameName
				);
				return nullptr;
			}

			if (entry->paths.has_value()) {
				return std::make_unique<ProfileBoundGameModule>(
					std::move(module),
					*entry->paths
				);
			}
			return module;
		}

		// Editor など未リンク App では、Paths だけ持つ既定モジュールで起動する。
		if (entry->paths.has_value()) {
			return std::make_unique<DefaultGameModule>(*entry->paths);
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
		const std::string startupScenePath = ResolveStartupScenePath(gameModule);
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
