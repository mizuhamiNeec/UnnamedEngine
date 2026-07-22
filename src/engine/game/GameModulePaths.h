#pragma once

#include <string>
#include <vector>

#include "core/filesystem/Path.h"
#include "core/filesystem/VirtualPath.h"

namespace Unnamed {
	/// @brief ゲームのルート情報と既定起動情報をまとめた構造体です。
	struct GameModulePaths {
		/// @brief ゲーム識別名です。
		std::string gameName;
		/// @brief ゲームルートディレクトリです。
		Path gameRoot;
		/// @brief コンテンツルートディレクトリです。
		Path contentRoot;
		/// @brief 設定ファイルルートディレクトリです。
		Path configRoot;
		/// @brief base マウント（低優先）の content ルート一覧です。
		std::vector<Path> baseContentMountRoots;
		/// @brief dlc マウント（中優先）の content ルート一覧です。
		std::vector<Path> dlcContentMountRoots;
		/// @brief mod マウント（高優先）の content ルート一覧です。
		std::vector<Path> modContentMountRoots;
		/// @brief 既定の起動シーン VirtualPath（contentRoot 相対）です。
		VirtualPath defaultStartupScene;
		/// @brief Runtime DLL のパス（gameRoot 基準または絶対）です。
		Path runtimeBinaryPath;
		/// @brief runtimeBinary を必須として扱うかどうかです。
		bool requireRuntimeBinary = false;
		/// @brief 静的登録より runtimeBinary を優先するかどうかです。
		bool preferRuntimeBinary = false;
		/// @brief 解決に成功した game_profile.json の実パスです。
		Path resolvedManifestPath;
	};
}
