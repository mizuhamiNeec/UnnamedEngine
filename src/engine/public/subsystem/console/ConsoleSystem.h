#pragma once

#include <memory>

#include <engine/public/time/DateTime.h>
#include <engine/public/subsystem/console/ConsoleUI.h>
#include <engine/public/subsystem/console/interface/IConsole.h>
#include <engine/public/subsystem/interface/ISubsystem.h>
#include <core/containers/RingBuffer.h>

namespace Unnamed {
	constexpr uint32_t kConsoleBufferSize = 1024;

	struct ConsoleLogText {
		LogLevel    level;
		std::string channel;
		std::string message;
		DateTime    timeStamp;
	};

	class ConsoleSystem final : public ISubsystem, public IConsole {
	public:
		~ConsoleSystem() override;

		// ISubsystem
		bool Init() override;
		void Update(float deltaTime) override;
		void Shutdown() override;

		[[nodiscard]] const std::string_view GetName() const override;

		// IConsole
		RingBuffer<ConsoleLogText, kConsoleBufferSize>& GetLogBuffer() {
			return mLogBuffer;
		}

		void Print(LogLevel         level, std::string_view channel,
		           std::string_view message) override;

	private:
		RingBuffer<ConsoleLogText, kConsoleBufferSize> mLogBuffer;

#ifdef _DEBUG
		std::unique_ptr<ConsoleUI> mConsoleUI;
#endif
	};
}
