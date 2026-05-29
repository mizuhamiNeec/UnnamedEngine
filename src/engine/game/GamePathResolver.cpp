#include "GamePathResolver.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "core/path/PathUtil.h"

namespace Unnamed {
	namespace {
		enum class ContentMountLayer {
			Base,
			Dlc,
			Mod,
		};

		struct ContentMountRoot {
			std::string       rootPath;
			ContentMountLayer layer = ContentMountLayer::Base;
			std::size_t       order = 0;
		};

		[[nodiscard]] bool IsAbsoluteOrCurrentRelative(
			const std::string_view path
		) {
			if (path.empty()) {
				return false;
			}
			const std::filesystem::path fsPath = Path::FromUtf8(path);
			return fsPath.is_absolute() || path.rfind("./", 0) == 0 ||
			       path.rfind("../", 0) == 0;
		}

		[[nodiscard]] bool IsRelativeToCurrentDir(const std::string_view path) {
			return path.rfind("./", 0) == 0 || path.rfind("../", 0) == 0;
		}

		[[nodiscard]] std::string NormalizePath(std::string_view path) {
			return Path::ToGenericUtf8(
				Path::FromUtf8(path).lexically_normal()
			);
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

			return Path::ToGenericUtf8(
				(Path::FromUtf8(gameRoot) / Path::FromUtf8(explicitRoot)).
				lexically_normal()
			);
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

			const std::filesystem::path fsPath = Path::FromUtf8(path);
			if (fsPath.is_absolute() || IsRelativeToCurrentDir(path)) {
				return NormalizePath(path);
			}

			if (root.empty()) {
				return NormalizePath(path);
			}

			return Path::ToGenericUtf8(
				(Path::FromUtf8(root) / Path::FromUtf8(path)).lexically_normal()
			);
		}

		[[nodiscard]] std::string ToString(const ContentMountLayer layer) {
			switch (layer) {
				case ContentMountLayer::Base:
					return "base";
				case ContentMountLayer::Dlc:
					return "dlc";
				case ContentMountLayer::Mod:
					return "mod";
			}
			return "unknown";
		}

		void AppendContentMountRoots(
			std::vector<ContentMountRoot>& outRoots,
			const std::vector<std::string>& inRoots,
			const ContentMountLayer layer
		) {
			for (std::size_t i = 0; i < inRoots.size(); ++i) {
				const std::string normalized = NormalizePath(inRoots[i]);
				if (normalized.empty()) {
					continue;
				}
				if (std::ranges::find_if(
					    outRoots,
					    [&](const ContentMountRoot& root) {
						    return root.rootPath == normalized;
					    }
				    ) != outRoots.end()) {
					continue;
				}
				outRoots.emplace_back(ContentMountRoot{
					.rootPath = normalized,
					.layer = layer,
					.order = i,
				});
			}
		}

		[[nodiscard]] std::vector<ContentMountRoot> BuildContentMountRoots(
			const GameModulePaths& paths
		) {
			std::vector<ContentMountRoot> roots;
			roots.reserve(
				paths.baseContentMountRoots.size() +
				paths.dlcContentMountRoots.size() +
				paths.modContentMountRoots.size() + 1
			);
			AppendContentMountRoots(
				roots,
				paths.baseContentMountRoots,
				ContentMountLayer::Base
			);
			AppendContentMountRoots(
				roots,
				paths.dlcContentMountRoots,
				ContentMountLayer::Dlc
			);
			AppendContentMountRoots(
				roots,
				paths.modContentMountRoots,
				ContentMountLayer::Mod
			);
			if (roots.empty()) {
				const std::string fallbackRoot = ResolveGameContentPath(paths, "");
				if (!fallbackRoot.empty()) {
					roots.emplace_back(ContentMountRoot{
						.rootPath = NormalizePath(fallbackRoot),
						.layer = ContentMountLayer::Base,
						.order = 0,
					});
				}
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
		return ResolveGameMountedContentPathDetailed(paths, path).resolvedPath;
	}

	MountedContentResolution ResolveGameMountedContentPathDetailed(
		const GameModulePaths& paths,
		std::string_view       path
	) {
		MountedContentResolution result = {};
		if (path.empty()) {
			result.resolvedPath = ResolveGameContentPath(paths, path);
			result.resolvedLayer = "base";
			result.existsOnDisk = true;
			return result;
		}
		if (IsAbsoluteOrCurrentRelative(path)) {
			result.resolvedPath = NormalizePath(path);
			result.resolvedLayer = "direct";
			std::error_code ec;
			result.existsOnDisk = Path::ExistsUtf8(result.resolvedPath, ec) && !ec;
			return result;
		}

		const std::vector<ContentMountRoot> mountRoots = BuildContentMountRoots(paths);
		if (mountRoots.empty()) {
			result.resolvedPath = ResolveGameContentPath(paths, path);
			result.resolvedLayer = "base";
			std::error_code ec;
			result.existsOnDisk = Path::ExistsUtf8(result.resolvedPath, ec) && !ec;
			return result;
		}

		std::error_code ec;
		for (auto it = mountRoots.rbegin(); it != mountRoots.rend(); ++it) {
			const std::string candidate = ResolveAgainstRoot(it->rootPath, path);
			if (candidate.empty()) {
				continue;
			}

			if (Path::ExistsUtf8(candidate, ec) && !ec) {
				result.resolvedPath = candidate;
				result.resolvedLayer = ToString(it->layer);
				result.resolvedRoot = it->rootPath;
				result.existsOnDisk = true;
				return result;
			}
			ec.clear();
		}

		result.resolvedPath = ResolveAgainstRoot(mountRoots.front().rootPath, path);
		result.resolvedLayer = ToString(mountRoots.front().layer);
		result.resolvedRoot = mountRoots.front().rootPath;
		result.existsOnDisk = false;
		return result;
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

	std::string ResolveStartupScenePath(
		const GameModulePaths& paths,
		std::string_view       startupScenePath
	) {
		std::string startupScene(startupScenePath);
		if (startupScene.empty()) {
			startupScene = paths.defaultStartupScene;
		}
		return ResolveGameMountedContentPath(paths, startupScene);
	}
}
