#include <string>

#include "core/filesystem/Path.h"

namespace Unnamed {
	/// @brief EditorGuiDataは、EditorGui assetのdecode結果をruntime生成処理へ渡す中間表現として保持します
	struct EditorGuiData {
		Path        sourcePath; // ソースファイルのパス
		std::string source;     // ソースコードの内容
		std::string lastError;  // 最後のエラーメッセージ...?
		bool        hasError;   // スクリプトにエラーがあるか?
	};
}
