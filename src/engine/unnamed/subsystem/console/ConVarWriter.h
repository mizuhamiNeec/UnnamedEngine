#pragma once

namespace Unnamed {
	/// @brief ConVarWriterは、永続化対象のコンソール変数を名前と値のテキスト行としてファイルへ書き出します
	class ConVarWriter {
	public:
		explicit ConVarWriter(const Path& path);
	};
}
