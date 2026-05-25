#pragma once

#include <string>
#include <vector>

namespace Unnamed {
	/// @brief ゲームのルート情報と既定起動情報をまとめた構造体です。
	struct GameModulePaths {
		/// @brief ゲーム識別名です。
		std::string gameName;
		/// @brief ゲームルートディレクトリです。
		std::string gameRoot;
		/// @brief コンテンツルートディレクトリです。
		std::string contentRoot;
		/// @brief 設定ファイルルートディレクトリです。
		std::string configRoot;
		/// @brief base マウント（低優先）の content ルート一覧です。
		std::vector<std::string> baseContentMountRoots;
		/// @brief dlc マウント（中優先）の content ルート一覧です。
		std::vector<std::string> dlcContentMountRoots;
		/// @brief mod マウント（高優先）の content ルート一覧です。
		std::vector<std::string> modContentMountRoots;
		/// @brief 既定の起動シーン（contentRoot 相対）です。
		std::string defaultStartupScene;
		/// @brief Runtime DLL のパス（gameRoot 基準または絶対）です。
		std::string runtimeBinaryPath;
		/// @brief runtimeBinary を必須として扱うかどうかです。
		bool requireRuntimeBinary = false;
		/// @brief 静的登録より runtimeBinary を優先するかどうかです。
		bool preferRuntimeBinary = false;
		/// @brief 解決に成功した game_profile.json の実パスです。
		std::string resolvedManifestPath;
	};
}
