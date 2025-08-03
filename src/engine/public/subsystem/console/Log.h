#pragma once
#include <format>
#include <string_view>
#include <iostream>

#include <engine/public/subsystem/console/ConsoleSystem.h>
#include <engine/public/subsystem/interface/ServiceLocator.h>
#include <engine/public/utils/UnnamedMacro.h>

template <typename... Args>
void Msg(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto* console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	if (!console) {
		// ServiceLocatorが無効な場合は標準出力に直接出力
		const std::string s = std::format(format, std::forward<Args>(args)...);
		std::string out = "[" + std::string(channel) + "] " + s;
		std::cout << out << "\n";
#ifdef _WIN32
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
#endif
		return;
	}

	const std::string s = std::format(format, std::forward<Args>(args)...);

	console->Print(
		Unnamed::LogLevel::Info,
		channel,
		s
	);
}

template <typename... Args>
void DevMsg(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto* console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	if (!console) {
		// ServiceLocatorが無効な場合は標準出力に直接出力
		const std::string s = std::format(format, std::forward<Args>(args)...);
		std::string out = "[" + std::string(channel) + "][DEV] " + s;
		std::cout << out << "\n";
#ifdef _WIN32
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
#endif
		return;
	}

	const std::string s = std::format(format, std::forward<Args>(args)...);

	console->Print(
		Unnamed::LogLevel::Dev,
		channel,
		s
	);
}

template <typename... Args>
void Warning(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto* console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	if (!console) {
		// ServiceLocatorが無効な場合は標準出力に直接出力
		const std::string s = std::format(format, std::forward<Args>(args)...);
		std::string out = "[" + std::string(channel) + "][WARNING] " + s;
		std::cout << out << "\n";
#ifdef _WIN32
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
#endif
		return;
	}

	const std::string s = std::format(format, std::forward<Args>(args)...);

	console->Print(
		Unnamed::LogLevel::Warning,
		channel,
		s
	);
}

template <typename... Args>
void Error(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto* console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	if (!console) {
		// ServiceLocatorが無効な場合は標準出力に直接出力
		const std::string s = std::format(format, std::forward<Args>(args)...);
		std::string out = "[" + std::string(channel) + "][ERROR] " + s;
		std::cout << out << "\n";
#ifdef _WIN32
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
#endif
		return;
	}

	const std::string s = std::format(format, std::forward<Args>(args)...);

	console->Print(
		Unnamed::LogLevel::Error,
		channel,
		s
	);
}

template <typename... Args>
void Fatal(
	const std::string_view&           channel,
	const std::format_string<Args...> format, Args&&... args
) {
	auto* console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	if (!console) {
		// ServiceLocatorが無効な場合は標準出力に直接出力
		const std::string s = std::format(format, std::forward<Args>(args)...);
		std::string out = "[" + std::string(channel) + "][FATAL] " + s;
		std::cout << out << "\n";
#ifdef _WIN32
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
#endif
		return;
	}

	const std::string s = std::format(format, std::forward<Args>(args)...);

	console->Print(
		Unnamed::LogLevel::Fatal,
		channel,
		s
	);
}

template <typename... Args>
void SpecialMsg(
	const Unnamed::LogLevel           logLevel, const std::string_view& channel,
	const std::format_string<Args...> format, Args&&...                 args
) {
	auto* console = ServiceLocator::Get<Unnamed::ConsoleSystem>();
	if (!console) {
		// ServiceLocatorが無効な場合は標準出力に直接出力
		const std::string s = std::format(format, std::forward<Args>(args)...);
		std::string out = "[" + std::string(channel) + "][SPECIAL] " + s;
		std::cout << out << "\n";
#ifdef _WIN32
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
#endif
		return;
	}

	const std::string s = std::format(format, std::forward<Args>(args)...);

	console->Print(
		logLevel,
		channel,
		s
	);
}
