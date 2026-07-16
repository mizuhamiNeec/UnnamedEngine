#include "GamePathResolver.h"

#include <algorithm>
#include <filesystem>
#include <vector>

#include "core/filesystem/Path.h"

namespace Unnamed {
	namespace {
		[[nodiscard]] Path ContentPathFromVirtualPath(
			const VirtualPath& virtualPath
		) {
			if (virtualPath.IsEmpty()) {
				return {};
			}
			return Path(virtualPath.String());
		}

		enum class CONTENT_MOUNT_LAYER {
			BASE,
			DLC,
			MOD,
		};

		struct ContentMountRoot {
			Path                rootPath;
			CONTENT_MOUNT_LAYER layer = CONTENT_MOUNT_LAYER::BASE;
			std::size_t         order = 0;
		};

		[[nodiscard]] bool IsAbsoluteOrCurrentRelative(
			const Path& path
		) {
			if (path.IsEmpty()) {
				return false;
			}

			const auto str = path.Native().string();

			return
				path.IsAbsolute() || path.IsRelative() && str.starts_with("./");
		}

		[[nodiscard]] bool IsRelativeToCurrentDir(const Path& path) {
			if (path.IsEmpty()) {
				return false;
			}

			const auto str = path.Native().string();

			return path.IsRelative() && str.starts_with("./");
		}

		[[nodiscard]] Path ResolveRootWithGameRootFallback(
			const Path& gameRoot,
			const Path& explicitRoot
		) {
			if (explicitRoot.IsEmpty()) {
				return gameRoot.LexicallyNormal();
			}

			if (IsAbsoluteOrCurrentRelative(explicitRoot)) {
				return explicitRoot.LexicallyNormal();
			}

			if (gameRoot.IsEmpty()) {
				return explicitRoot.LexicallyNormal();
			}

			return gameRoot / explicitRoot.LexicallyNormal();
		}

		[[nodiscard]] Path ResolveAgainstRoot(
			const Path& root,
			const Path& path
		) {
			if (path.IsEmpty()) {
				if (root.IsEmpty()) {
					return {};
				}
				return root.LexicallyNormal();
			}

			const auto& fsPath = path.Native();

			if (fsPath.is_absolute() || IsRelativeToCurrentDir(path)) {
				return path.LexicallyNormal();
			}

			if (root.IsEmpty()) {
				return path.LexicallyNormal();
			}

			return root / path.LexicallyNormal();
		}

		[[nodiscard]] std::string_view ToString(
			const CONTENT_MOUNT_LAYER layer
		) {
			switch (layer) {
				case CONTENT_MOUNT_LAYER::BASE: return "base";
				case CONTENT_MOUNT_LAYER::DLC: return "dlc";
				case CONTENT_MOUNT_LAYER::MOD: return "mod";
			}
			return "unknown";
		}

		void AppendContentMountRoots(
			std::vector<ContentMountRoot>& outRoots,
			const std::vector<Path>&       inRoots,
			const CONTENT_MOUNT_LAYER      layer
		) {
			for (std::size_t i = 0; i < inRoots.size(); ++i) {
				auto normalized = inRoots[i].LexicallyNormal();
				if (normalized.IsEmpty()) {
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
				outRoots.emplace_back(
					ContentMountRoot{
						.rootPath = normalized,
						.layer    = layer,
						.order    = i,
					}
				);
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
				CONTENT_MOUNT_LAYER::BASE
			);
			AppendContentMountRoots(
				roots,
				paths.dlcContentMountRoots,
				CONTENT_MOUNT_LAYER::DLC
			);
			AppendContentMountRoots(
				roots,
				paths.modContentMountRoots,
				CONTENT_MOUNT_LAYER::MOD
			);
			if (roots.empty()) {
				const auto fallbackRoot = ResolveGameContentPath(paths, {});
				if (!fallbackRoot.IsEmpty()) {
					roots.emplace_back(ContentMountRoot{
						.rootPath = fallbackRoot,
						.layer    = CONTENT_MOUNT_LAYER::BASE,
						.order    = 0,
					});
				}
			}
			return roots;
		}
	}

	Path ResolveGameRootPath(
		const GameModulePaths& paths,
		const Path&            path
	) {
		return ResolveAgainstRoot(paths.gameRoot, path);
	}

	Path ResolveGameContentPath(
		const GameModulePaths& paths,
		const Path&            path
	) {
		const Path contentRoot = ResolveRootWithGameRootFallback(
			paths.gameRoot,
			paths.contentRoot
		);
		return ResolveAgainstRoot(contentRoot, path);
	}

	Path ResolveGameMountedContentPath(
		const GameModulePaths& paths,
		const Path&            path
	) {
		return ResolveGameMountedContentPathDetailed(paths, path).resolvedPath;
	}

	Path ResolveGameMountedContentPath(
		const GameModulePaths& paths,
		const VirtualPath&     virtualPath
	) {
		return ResolveGameMountedContentPathDetailed(
			paths,
			virtualPath
		).resolvedPath;
	}

	MountedContentResolution ResolveGameMountedContentPathDetailed(
		const GameModulePaths& paths,
		const Path&            path
	) {
		MountedContentResolution result = {};
		if (path.IsEmpty()) {
			result.resolvedPath  = ResolveGameContentPath(paths, path);
			result.resolvedLayer = "base";
			result.existsOnDisk  = true;
			return result;
		}
		// 明示された物理パスはコンテンツの上書き検索を通さない
		if (IsAbsoluteOrCurrentRelative(path)) {
			result.resolvedPath  = path.LexicallyNormal();
			result.resolvedLayer = "direct";
			result.existsOnDisk  = result.resolvedPath.Exists();
			return result;
		}

		const std::vector<ContentMountRoot> mountRoots =
			BuildContentMountRoots(paths);
		if (mountRoots.empty()) {
			result.resolvedPath  = ResolveGameContentPath(paths, path);
			result.resolvedLayer = "base";
			result.existsOnDisk  = result.resolvedPath.Exists();
			return result;
		}

		std::error_code ec;
		// base < DLC < mod の順に、より後段のコンテンツで上書きする
		for (auto it = mountRoots.rbegin(); it != mountRoots.rend(); ++it) {
			auto candidate = ResolveAgainstRoot(it->rootPath, path);
			if (candidate.IsEmpty()) {
				continue;
			}

			if (candidate.Exists()) {
				result.resolvedPath  = candidate;
				result.resolvedLayer = ToString(it->layer);
				result.resolvedRoot  = it->rootPath;
				result.existsOnDisk  = true;
				return result;
			}
			ec.clear();
		}

		result.resolvedPath = ResolveAgainstRoot(
			mountRoots.front().rootPath, path);
		result.resolvedLayer = ToString(mountRoots.front().layer);
		result.resolvedRoot  = mountRoots.front().rootPath;
		result.existsOnDisk  = false;
		return result;
	}

	MountedContentResolution ResolveGameMountedContentPathDetailed(
		const GameModulePaths& paths,
		const VirtualPath&     virtualPath
	) {
		MountedContentResolution result =
			ResolveGameMountedContentPathDetailed(
				paths,
				ContentPathFromVirtualPath(virtualPath)
			);
		result.virtualPath = virtualPath;
		return result;
	}

	Path ResolveGameConfigPath(
		const GameModulePaths& paths,
		const Path&            path
	) {
		const Path configRoot = ResolveRootWithGameRootFallback(
			paths.gameRoot,
			paths.configRoot
		);
		return ResolveAgainstRoot(configRoot, path);
	}

	MountedContentResolution ResolveStartupScenePathDetailed(
		const GameRuntimeContext& runtimeContext
	) {
		return ResolveGameMountedContentPathDetailed(
			runtimeContext.modulePaths,
			runtimeContext.defaultStartupScene
		);
	}

	Path ResolveStartupScenePath(
		const GameRuntimeContext& runtimeContext
	) {
		return ResolveStartupScenePathDetailed(runtimeContext).resolvedPath;
	}
}
