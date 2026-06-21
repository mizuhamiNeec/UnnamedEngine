#pragma once

#include <string>

#include "engine/game/GameModulePaths.h"

namespace Unnamed {
	/// @brief ランタイム選択済みゲーム情報を Engine へ渡すコンテキストです。
	struct GameRuntimeContext {
		/// @brief 選択された runtime module 名です。
		std::string runtimeModuleName;
		/// @brief ゲームのルート情報と既定起動情報です。
		GameModulePaths modulePaths = {};
		/// @brief 起動時デフォルトシーン（content 相対）です。
		Path defaultStartupScenePath;
		/// @brief UI ドキュメントのデフォルトパスです。
		Path defaultUiDocumentPath;
	};
}
