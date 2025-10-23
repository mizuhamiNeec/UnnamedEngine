#pragma once
#include <format>
#include <string_view>
#include <iostream>

#include <engine/subsystem/console/ConsoleSystem.h>
#include <engine/subsystem/interface/ServiceLocator.h>
#include <core/UnnamedMacro.h>

namespace {
	/// @brief メッセージを出力します（フォールバック用）
	/// @param level ログレベル
	/// @param channel チャンネル名
	/// @param message メッセージ本文
	void Print(
		const Unnamed::LogLevel level,
		const std::string_view  channel,
		const std::string_view  message
	) {
		std::string out;

		// レベル・チャンネルが空の場合はチャンネル名を出力しない
		if (level != Unnamed::LogLevel::None && !Unnamed::kChannelNone.
			empty()) {
			out =
				"[" +
				std::string(channel) +
				"] " +
				std::string(message);
		} else {
			out = std::string(message);
		}

		// コンソールの出力
		std::cout << out << "\n";

		// VSの出力
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
	}

	/// @brief パスからベースネームを取得します
	/// @param path ファイルパス
	/// @return ベースネーム
	constexpr std::string_view BaseName(std::string_view path) {
		const size_t pos1 = path.find_last_of('/');
		const size_t pos2 = path.find_last_of('\\');
		const size_t pos  = (pos1 == std::string_view::npos) ?
			                   pos2 :
			                   (pos2 == std::string_view::npos) ?
			                   pos1 :
			                   std::max(pos1, pos2);
		return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
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
		const Unnamed::LogLevel           level,
		const std::string_view&           channel,
		const std::source_location        location,
		const std::format_string<Args...> fmt, Args&&... args
	) {
		auto* console = ServiceLocator::Get<Unnamed::ConsoleSystem>();

		const std::string body = std::format(fmt, std::forward<Args>(args)...);

		if (!console) {
			// ServiceLocator無効時のフォールバック
			Print(level, channel, body);
			return;
		}
		console->Print(level, channel, body, location);
	}
}

#define Msg(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Info, channel, std::source_location::current(), format, ##__VA_ARGS__)

#define DevMsg(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Dev, channel, std::source_location::current(), format, ##__VA_ARGS__)

#define Warning(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Warning, channel, std::source_location::current(), format, ##__VA_ARGS__)

#define Error(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Error, channel, std::source_location::current(), format, ##__VA_ARGS__)

#define Fatal(channel, format, ...) \
	LogCore(Unnamed::LogLevel::Fatal, channel, std::source_location::current(), format, ##__VA_ARGS__)

#define SpecialMsg(logLevel, channel, format, ...) \
	LogCore(logLevel, channel, std::source_location::current(), format, ##__VA_ARGS__)
