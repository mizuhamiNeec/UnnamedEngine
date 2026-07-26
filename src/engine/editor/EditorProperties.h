#pragma once

namespace Unnamed {
	//-------------------------------------------------------------------------
	// エディタのプロパティ
	//-------------------------------------------------------------------------

	/// @brief EditorUIPropertiesは、Editor UIで共有する色、余白、寸法のstyle値を保持します
	struct EditorUIProperties {
		float titleBarHeight = 36;   // タイトルバーの高さ
		float popupPadding   = 8.0f; // ポップアップのパディング
	};
}
