#pragma once
#include <format>
#include <iostream>
#include <string_view>

#include <Windows.h>

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	/// @brief メッセージを出力します（フォールバック用）
	/// @param level ログレベル
	/// @param channel チャンネル名
	/// @param message メッセージ本文
	inline void Print(
		const LogLevel         level,
		const std::string_view channel,
		const std::string_view message
	) {
		std::string out;

		// レベル・チャンネルが空の場合はチャンネル名を出力しない
		if (
			level != LogLevel::None && !kChannelNone.empty()
		) {
			out =
				"[" +
				std::string(channel) +
				"] " +
				std::string(message);
		}
		out = std::string(message);

		// コンソールの出力
		std::cout << out << "\n";

		// VSの出力
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
	}

	/// @brief パスからベースネームを取得します
	/// @param path ファイルパス
	/// @return ベースネーム
	constexpr std::string_view BaseName(const std::string_view path) {
		const size_t pos1 = path.find_last_of('/');
		const size_t pos2 = path.find_last_of('\\');
		const size_t pos  = pos1 == std::string_view::npos ?
			                   pos2 :
			                   pos2 == std::string_view::npos ?
			                   pos1 :
			                   std::max(pos1, pos2);
		return pos == std::string_view::npos ? path : path.substr(pos + 1);
	}

	/// @brief コアログ出力関数
	/// @tparam Args フォーマット引数の型パック
	/// @param level ログレベル
	/// @param channel チャンネル名
	/// @param location ソース位置情報
	/// @param fmt フォーマット文字列
	/// @param args フォーマット引数
	template <typename... Args>
	void LogCore(
		const LogLevel                    level,
		const std::string_view&           channel,
		const std::source_location        location,
		const std::format_string<Args...> fmt, Args&&... args
	) {
		auto* console = ServiceLocator::Get<ConsoleSystem>();

		const std::string body = std::format(fmt, std::forward<Args>(args)...);

		if (!console) {
			// ServiceLocatorが使えない時のフォールバック
			Print(level, channel, body);
			return;
		}

		console->Print(level, channel, body, location);

		// 警告以上のレベルのメッセージはエディタ通知も出す
		switch (level) {
			case LogLevel::Warning: console->ExecuteCommand(
					"notify warning 2 Warning | " + body,
					EXEC_FLAG::FROM_CONSOLE | EXEC_FLAG::SILENT
				);
				break; // わすれんじゃあねぇ
			case LogLevel::Error: console->ExecuteCommand(
					"notify error 2 Error | " + body,
					EXEC_FLAG::FROM_CONSOLE | EXEC_FLAG::SILENT
				);
				break;
			case LogLevel::Fatal: console->ExecuteCommand(
					"notify fatal 10 Fatal | " + body,
					EXEC_FLAG::FROM_CONSOLE | EXEC_FLAG::SILENT
				);
				break;
			default: ;
		}
	}
}

/// @brief 情報メッセージを出力します。
/// @param channel チャンネル名
/// @param format フォーマット文字列
/// @param args フォーマット引数
#define Msg(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Info, channel, std::source_location::current(), format, ##__VA_ARGS__)

/// @brief 開発用の詳細なメッセージを出力します。リリースビルドでは出力されないことが多いです。
/// @param channel チャンネル名
/// @param format フォーマット文字列
/// @param args フォーマット引数
#define DevMsg(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Dev, channel, std::source_location::current(), format, ##__VA_ARGS__)

/// @brief 警告メッセージを出力します。注意が必要な状況を示すのに使います。
/// @param channel チャンネル名
/// @param format フォーマット文字列
/// @param args フォーマット引数
#define Warning(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Warning, channel, std::source_location::current(), format, ##__VA_ARGS__)

/// @brief エラーメッセージを出力します。問題が発生したことを示すのに使います。
/// @param channel チャンネル名
/// @param format フォーマット文字列
/// @param args フォーマット引数
#define Error(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Error, channel, std::source_location::current(), format, ##__VA_ARGS__)

/// @brief 致命的なエラーメッセージを出力します。まじでヤベー状況を示すのに使います。あんまり見たくないです。ははっ。
/// @param channel チャンネル名
/// @param format フォーマット文字列
/// @param args フォーマット引数
#define Fatal(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Fatal, channel, std::source_location::current(), format, ##__VA_ARGS__)

/// @brief 特殊なログメッセージを出力します。用途に応じてログレベルを指定できます。
/// @param logLevel ログレベル
/// @param channel チャンネル名
/// @param format フォーマット文字列
/// @param args フォーマット引数
#define SpecialMsg(logLevel, channel, format, ...) \
	LogCore(logLevel, channel, std::source_location::current(), format, ##__VA_ARGS__)
