#include <string>

#include "core/filesystem/Path.h"

namespace Unnamed {
	struct EditorGuiData {
		Path        sourcePath; // ソースファイルのパス
		std::string source;     // ソースコードの内容
		std::string lastError;  // 最後のエラーメッセージ...?
		bool        hasError;   // スクリプトにエラーがあるか?
	};
}
