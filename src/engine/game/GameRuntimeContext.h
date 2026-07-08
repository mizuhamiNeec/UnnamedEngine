#pragma once

#include <optional>
#include <string>

#include "engine/game/GameModulePaths.h"

namespace Unnamed {
	/// @brief ランタイム選択済みゲーム情報を Engine へ渡すコンテキストです。
	struct GameRuntimeContext final {
		/// @brief 選択された runtime module 名です。
		std::string runtimeModuleName;
		/// @brief ゲームのルート情報と既定起動情報です。
		GameModulePaths modulePaths = {};
		/// @brief 起動時デフォルトシーンの論理 VirtualPath です。
		VirtualPath defaultStartupScene;
		/// @brief UI ドキュメントのデフォルト論理パスです。
		std::optional<VirtualPath> defaultUiDocument;
	};
}
