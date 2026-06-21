#pragma once

#include <string>
#include <string_view>

#include "engine/game/GameModulePaths.h"

namespace Unnamed {
	/// @brief マウント付き content 解決の詳細結果です。
	struct MountedContentResolution {
		/// @brief 解決済みパスです。
		Path resolvedPath;
		/// @brief 選択されたマウントレイヤー名です（base/dlc/mod/direct）。
		std::string resolvedLayer;
		/// @brief 選択されたマウントルートです。
		Path resolvedRoot;
		/// @brief 解決時点で実在ファイルを検出できたかどうかです。
		bool existsOnDisk = false;
	};

	/// @brief Game root 基準でパスを解決します。
	[[nodiscard]] Path ResolveGameRootPath(
		const GameModulePaths& paths,
		const Path&            path
	);

	/// @brief Game content root 基準でパスを解決します。
	/// @details contentRoot が空の場合は gameRoot を基準として解決します。
	[[nodiscard]] Path ResolveGameContentPath(
		const GameModulePaths& paths,
		const Path& path
	);

	/// @brief マウント順（base -> dlc -> mod）を考慮して content パスを解決します。
	/// @details 相対パスは後勝ち（mod 優先）で実在ファイルを探索し、未発見時は base 先頭へフォールバックします。
	[[nodiscard]] Path ResolveGameMountedContentPath(
		const GameModulePaths& paths,
		const Path&            path
	);

	/// @brief マウント順（base -> dlc -> mod）を考慮して content パスを詳細付きで解決します。
	[[nodiscard]] MountedContentResolution
	ResolveGameMountedContentPathDetailed(
		const GameModulePaths& paths,
		const Path&            path
	);

	/// @brief Game config root 基準でパスを解決します。
	/// @details configRoot が空の場合は gameRoot を基準として解決します。
	[[nodiscard]] Path ResolveGameConfigPath(
		const GameModulePaths& paths,
		const Path&            path
	);

	/// @brief 既定起動シーンパスを content root 基準で解決します。
	[[nodiscard]] Path ResolveStartupScenePath(
		const GameModulePaths& paths,
		const Path&            startupScenePath
	);
}
