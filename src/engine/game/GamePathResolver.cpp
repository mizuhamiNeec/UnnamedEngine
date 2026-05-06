#include "GamePathResolver.h"

#include <algorithm>
#include <filesystem>
#include <vector>

namespace Unnamed {
	namespace {
		[[nodiscard]] bool IsAbsoluteOrCurrentRelative(
			const std::string_view path
		) {
			if (path.empty()) {
				return false;
			}
			const std::filesystem::path fsPath(path);
			return fsPath.is_absolute() || path.rfind("./", 0) == 0 ||
			       path.rfind("../", 0) == 0;
		}

		[[nodiscard]] bool IsRelativeToCurrentDir(const std::string_view path) {
			return path.rfind("./", 0) == 0 || path.rfind("../", 0) == 0;
		}

		[[nodiscard]] std::string NormalizePath(std::string_view path) {
			return std::filesystem::path(path).lexically_normal().generic_string();
		}

		[[nodiscard]] std::string ResolveRootWithGameRootFallback(
			const std::string_view gameRoot,
			const std::string_view explicitRoot
		) {
			if (explicitRoot.empty()) {
				return NormalizePath(gameRoot);
			}

			if (IsAbsoluteOrCurrentRelative(explicitRoot)) {
				return NormalizePath(explicitRoot);
			}

			if (gameRoot.empty()) {
				return NormalizePath(explicitRoot);
			}

			return std::filesystem::path(gameRoot)
			       .append(explicitRoot)
			       .lexically_normal()
			       .generic_string();
		}

		[[nodiscard]] std::string ResolveAgainstRoot(
			const std::string_view root,
			const std::string_view path
		) {
			if (path.empty()) {
				if (root.empty()) {
					return {};
				}
				return NormalizePath(root);
			}

			const std::filesystem::path fsPath(path);
			if (fsPath.is_absolute() || IsRelativeToCurrentDir(path)) {
				return NormalizePath(path);
			}

			if (root.empty()) {
				return NormalizePath(path);
			}

			return std::filesystem::path(root)
			       .append(path)
			       .lexically_normal()
			       .generic_string();
		}

		[[nodiscard]] std::vector<std::string> BuildContentMountRoots(
			const GameModulePaths& paths
		) {
			std::vector<std::string> roots;
			roots.reserve(
				paths.baseContentMountRoots.size() +
				paths.dlcContentMountRoots.size() +
				paths.modContentMountRoots.size() + 1
			);
			const auto appendUniqueRoot = [&](const std::string& root) {
				const std::string normalized = NormalizePath(root);
				if (normalized.empty()) {
					return;
				}
				if (std::ranges::find(roots, normalized) != roots.end()) {
					return;
				}
				roots.emplace_back(normalized);
			};
			for (const std::string& root : paths.baseContentMountRoots) {
				appendUniqueRoot(root);
			}
			for (const std::string& root : paths.dlcContentMountRoots) {
				appendUniqueRoot(root);
			}
			for (const std::string& root : paths.modContentMountRoots) {
				appendUniqueRoot(root);
			}
			if (roots.empty()) {
				appendUniqueRoot(ResolveGameContentPath(paths, ""));
			}
			return roots;
		}
	}

	std::string ResolveGameRootPath(
		const GameModulePaths& paths,
		std::string_view       path
	) {
		return ResolveAgainstRoot(paths.gameRoot, path);
	}

	std::string ResolveGameContentPath(
		const GameModulePaths& paths,
		std::string_view       path
	) {
		const std::string contentRoot = ResolveRootWithGameRootFallback(
			paths.gameRoot,
			paths.contentRoot
		);
		return ResolveAgainstRoot(contentRoot, path);
	}

	std::string ResolveGameMountedContentPath(
		const GameModulePaths& paths,
		std::string_view       path
	) {
		if (path.empty()) {
			return ResolveGameContentPath(paths, path);
		}
		if (IsAbsoluteOrCurrentRelative(path)) {
			return NormalizePath(path);
		}

		const std::vector<std::string> mountRoots = BuildContentMountRoots(paths);
		if (mountRoots.empty()) {
			return ResolveGameContentPath(paths, path);
		}

		std::error_code ec;
		for (auto it = mountRoots.rbegin(); it != mountRoots.rend(); ++it) {
			const std::string candidate = ResolveAgainstRoot(*it, path);
			if (candidate.empty()) {
				continue;
			}

			if (std::filesystem::exists(candidate, ec) && !ec) {
				return candidate;
			}
			ec.clear();
		}

		return ResolveAgainstRoot(mountRoots.front(), path);
	}

	std::string ResolveGameConfigPath(
		const GameModulePaths& paths,
		std::string_view       path
	) {
		const std::string configRoot = ResolveRootWithGameRootFallback(
			paths.gameRoot,
			paths.configRoot
		);
		return ResolveAgainstRoot(configRoot, path);
	}

	std::string ResolveStartupScenePath(const IGameModule& gameModule) {
		const GameModulePaths paths = gameModule.GetGameModulePaths();
		std::string startupScene = gameModule.GetDefaultStartupScenePath();
		if (startupScene.empty()) {
			startupScene = paths.defaultStartupScene;
		}
		return ResolveGameMountedContentPath(paths, startupScene);
	}
}
