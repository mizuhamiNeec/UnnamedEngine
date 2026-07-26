#pragma once

#include <string>

#include <engine/platform/WindowsUtils.h>
#include <engine/unnamed/subsystem/interface/ISubsystem.h>

namespace Unnamed {
	class ConsoleSystem;

	/// @brief TerminalSystemは、Windows terminalの入出力をConsoleSystemのcommand実行へ接続します
	class TerminalSystem final : public ISubsystem {
	public:
		explicit TerminalSystem(ConsoleSystem* console);
		~TerminalSystem() override;

		bool Init() override;
		void Update(float deltaTime) override;
		void Shutdown() override;

		[[nodiscard]] const std::string_view GetName() const override {
			return "Terminal";
		}

		// 実行制御
		bool StartCmd(const std::string& commandLine);
		bool StartPowerShellFile(
			const std::string& scriptPath, const std::string& args
		);
		void Stop();

		// 状態
		[[nodiscard]] bool IsRunning() const {
			return mRunning;
		}

		[[nodiscard]] unsigned long GetExitCode() const {
			return mExitCode;
		}

		[[nodiscard]] unsigned long GetWin32Error() const {
			return mWin32Error;
		}

		[[nodiscard]] const std::string& GetOutputUtf8() const {
			return mOutputUtf8;
		}

		void ClearOutput();

	private:
		bool StartProcess(const std::wstring& app, const std::wstring& args);

		ConsoleSystem* mConsole = nullptr;

		bool                        mRunning = false;
		WindowsUtils::ProcessHandle mHandle{};
		std::string                 mOutputUtf8;
		unsigned long               mExitCode   = 0;
		unsigned long               mWin32Error = 0;
	};
}
