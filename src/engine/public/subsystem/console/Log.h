#pragma once
#include <format>
#include <string_view>
#include <iostream>

#include <engine/public/subsystem/console/ConsoleSystem.h>
#include <engine/public/subsystem/interface/ServiceLocator.h>
#include <core/UnnamedMacro.h>

void Print(
	Unnamed::LogLevel level,
	std::string_view  channel,
	std::string_view  message
);

inline void Print(
	const Unnamed::LogLevel level,
	const std::string_view  channel,
	const std::string_view  message
) {
	std::string out;

	// レベル・チャンネルがNoneの場合はチャンネル名を出力しない
	if (level != Unnamed::LogLevel::None && channel != "None") {
		out =
			"[" +
			std::string(channel) +
			"] " +
			std::string(message);
	} else {
		out =
			std::string(message);
	}

	// コンソールの出力
	std::cout << out << "\n";

	// VSの出力
	OutputDebugStringA(out.data());
	OutputDebugStringA("\n");
}

template <typename... Args>
void Msg(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto*             console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	const std::string s = std::format(format, std::forward<Args>(args)...);
	if (!console) {
		// ServiceLocator無効時のフォールバック
		Print(Unnamed::LogLevel::Info, channel, s);
		return;
	}

	console->Print(Unnamed::LogLevel::Info, channel, s);
}

template <typename... Args>
void DevMsg(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto*             console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	const std::string s = std::format(format, std::forward<Args>(args)...);
	if (!console) {
		// ServiceLocator無効時のフォールバック
		Print(Unnamed::LogLevel::Dev, channel, s);
		return;
	}

	console->Print(Unnamed::LogLevel::Dev, channel, s);
}

template <typename... Args>
void Warning(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto*             console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	const std::string s = std::format(format, std::forward<Args>(args)...);
	if (!console) {
		// ServiceLocator無効時のフォールバック
		Print(Unnamed::LogLevel::Warning, channel, s);
		return;
	}

	console->Print(Unnamed::LogLevel::Warning, channel, s);
}

template <typename... Args>
void Error(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto*             console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	const std::string s = std::format(format, std::forward<Args>(args)...);
	if (!console) {
		// ServiceLocator無効時のフォールバック
		Print(Unnamed::LogLevel::Error, channel, s);
		return;
	}

	console->Print(Unnamed::LogLevel::Error, channel, s);
}

template <typename... Args>
void Fatal(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto*             console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	const std::string s = std::format(format, std::forward<Args>(args)...);
	if (!console) {
		// ServiceLocator無効時のフォールバック
		Print(Unnamed::LogLevel::Fatal, channel, s);
		return;
	}

	console->Print(Unnamed::LogLevel::Fatal, channel, s);
}

template <typename... Args>
void SpecialMsg(
	const Unnamed::LogLevel           logLevel, const std::string_view& channel,
	const std::format_string<Args...> format, Args&&...                 args
) {
	auto*             console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	const std::string s = std::format(format, std::forward<Args>(args)...);
	if (!console) {
		// ServiceLocator無効時のフォールバック
		Print(logLevel, channel, s);
		return;
	}

	console->Print(logLevel, channel, s);
}
