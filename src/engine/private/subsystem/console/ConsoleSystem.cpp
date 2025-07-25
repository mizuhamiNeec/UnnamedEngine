#include <pch.h>

#include <iostream>

#include <engine/public/subsystem/console/ConsoleSystem.h>

#include <engine/public/subsystem/interface/ServiceLocator.h>

namespace Unnamed {
	ConsoleSystem::~ConsoleSystem() = default;

	bool ConsoleSystem::Init() {
		ServiceLocator::Register<IConsole>(this);

		OutputDebugStringW(L"ConsoleSystem Initialized\n");
		return true;
	}

	void ConsoleSystem::Update(const float deltaTime) {
		deltaTime;
	}

	void ConsoleSystem::Shutdown() {
	}

	const std::string_view ConsoleSystem::GetName() const {
		return "Console";
	}

	void ConsoleSystem::Print(
		const LogLevel         level,
		const std::string_view channel,
		const std::string_view message
	) {
		// ログメッセージをバッファに追加
		ConsoleLogText logText;
		logText.level   = level;
		logText.channel = channel;
		logText.message = message;
		mLogBuffer.Push(logText);

		// コンソールの出力
		std::cout << message << "\n";

		// Visual Studioの出力
		OutputDebugStringA(message.data());
		OutputDebugStringA("\n");
	}
}
