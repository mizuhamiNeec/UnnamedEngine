#pragma once

namespace Unnamed {
	/// @brief コンソールスクリプトパーサークラス
	/// @details コンソールスクリプト(.cfg)を解析してコマンドを実行するクラス
	class ConsoleScriptParser final {
	public:
		static void ParseAndExecute(const Path& path);
	};
}
