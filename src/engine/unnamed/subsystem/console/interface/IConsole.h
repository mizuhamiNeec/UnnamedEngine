#pragma once

#include <string_view>

namespace Unnamed {
	constexpr std::string_view kChannelNone = "";

	enum class LogLevel {
		None = 0, // 純粋なテキストのみを出力したい場合に使用します。
		Info,
		Dev,     // developer の有効時に出力されます。
		Warning, // 警告メッセージ
		Error,   // エラーメッセージ
		Fatal,   // 致命的なエラーメッセージ

		// 以下は特定用途向け

		Execute, // コマンドの実行時に使用されます。
		Waiting, // コマンドの実行待機時に使用されます。
		Success, // 成功メッセージ

		Bool,   // 真偽値用
		Int,    // 整数用
		Float,  // 浮動小数点数用
		Double, // 倍精度浮動小数点数用
		String, // 文字列用
		Vec3,   // 3次元ベクトル用
	};

	const char* ToString(LogLevel e);

	/// @brief コンソールインターフェースクラス
	class IConsole {
	public:
		virtual      ~IConsole() = default;
		virtual void Print(
			LogLevel             level,
			std::string_view     channel,
			std::string_view     message,
			std::source_location location
		) = 0;
	};
}
