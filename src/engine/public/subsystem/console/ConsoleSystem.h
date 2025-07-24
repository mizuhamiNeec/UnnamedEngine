#pragma once
#include <engine/public/subsystem/console/interface/IConsole.h>
#include <engine/public/subsystem/interface/ISubsystem.h>
#include <engine/public/utils/container/RingBuffer.h>

namespace Unnamed {
	constexpr uint32_t kConsoleBufferSize = 1024;

	struct ConsoleLogText {
		LogLevel    level;
		std::string channel;
		std::string message;
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
		void Print(LogLevel         level, std::string_view channel,
		           std::string_view message) override;

	private:
		RingBuffer<ConsoleLogText, kConsoleBufferSize> mLogBuffer;
	};
}
