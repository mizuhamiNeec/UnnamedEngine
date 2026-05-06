#pragma once

#include <string>
#include <string_view>

#include "engine/game/IGameModule.h"

namespace Unnamed {
	/// @brief Game root 基準でパスを解決します。
	[[nodiscard]] std::string ResolveGameRootPath(
		const GameModulePaths& paths,
		std::string_view       path
	);

	/// @brief Game content root 基準でパスを解決します。
	/// @details contentRoot が空の場合は gameRoot を基準として解決します。
	[[nodiscard]] std::string ResolveGameContentPath(
		const GameModulePaths& paths,
		std::string_view       path
	);

	/// @brief マウント順（base -> dlc -> mod）を考慮して content パスを解決します。
	/// @details 相対パスは後勝ち（mod 優先）で実在ファイルを探索し、未発見時は base 先頭へフォールバックします。
	[[nodiscard]] std::string ResolveGameMountedContentPath(
		const GameModulePaths& paths,
		std::string_view       path
	);

	/// @brief Game config root 基準でパスを解決します。
	/// @details configRoot が空の場合は gameRoot を基準として解決します。
	[[nodiscard]] std::string ResolveGameConfigPath(
		const GameModulePaths& paths,
		std::string_view       path
	);

	/// @brief GameModule の既定起動シーンパスを content root 基準で解決します。
	[[nodiscard]] std::string ResolveStartupScenePath(
		const IGameModule& gameModule
	);
}
