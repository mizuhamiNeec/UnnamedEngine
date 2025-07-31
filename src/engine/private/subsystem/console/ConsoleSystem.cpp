#include <pch.h>
//-----------------------------------------------------------------------------
#include <iostream>

#include <engine/public/subsystem/console/ConsoleSystem.h>
#include <engine/public/subsystem/console/Log.h>
#include <engine/public/subsystem/interface/ServiceLocator.h>
#include <engine/public/subsystem/time/SystemClock.h>

namespace Unnamed {
	ConsoleSystem::~ConsoleSystem() = default;

	bool ConsoleSystem::Init() {
		ServiceLocator::Register<ConsoleSystem>(this);
#ifdef _DEBUG
		mConsoleUI = std::make_unique<ConsoleUI>(this);
#endif

		return true;
	}

	void ConsoleSystem::Update(float) {
#ifdef _DEBUG
		mConsoleUI->Show();
#endif
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
		logText.level     = level;
		logText.channel   = channel;
		logText.message   = message;
		logText.timeStamp = SystemClock::GetDateTime(SystemClock::Now());
		mLogBuffer.Push(logText);

		std::string out =
			"[" +
			std::string(channel) +
			"] " +
			std::string(message);

		// コンソールの出力
		std::cout << out << "\n";

		// Visual Studioの出力
		OutputDebugStringA(out.data());
		OutputDebugStringA("\n");
	}
}
