#include "TerminalSystem.h"

#include <Windows.h>

#include <engine/unnamed/subsystem/console/ConsoleSystem.h>

#include "core/string/StrUtil.h"

namespace Unnamed {
	TerminalSystem::TerminalSystem(ConsoleSystem* console) : mConsole(
		console
	) {
	}

	TerminalSystem::~TerminalSystem() {
		Stop();
	}

	bool TerminalSystem::Init() {
		return true;
	}

	void TerminalSystem::Update(float) {
		if (!mRunning) {
			return;
		}

		std::string chunk;
		while (WindowsUtils::PollProcessOutput(mHandle, chunk)) {
			mOutputUtf8 += WindowsUtils::ConvertCP932ToUtf8(chunk);
		}

		unsigned long code = 0;
		if (WindowsUtils::TryGetProcessExitCode(mHandle, code)) {
			mExitCode = code;
			mRunning  = false;
			WindowsUtils::CloseProcessHandle(mHandle);
		}
	}

	void TerminalSystem::Shutdown() {
		Stop();
	}

	bool TerminalSystem::StartCmd(const std::string& commandLine) {
		// cmd 内部コマンド含めて動かす
		const std::wstring cmdW = StrUtil::ToWString(commandLine);
		const std::wstring args = L"/S /C \"" + cmdW + L"\"";
		return StartProcess(L"cmd.exe", args);
	}

	bool TerminalSystem::StartPowerShellFile(
		const std::string& scriptPath, const std::string& argsUtf8
	) {
		const std::wstring scriptW = StrUtil::ToWString(scriptPath);
		const std::wstring argsW   = StrUtil::ToWString(argsUtf8);

		std::wstring psArgs = L"-NoProfile -ExecutionPolicy Bypass -File \"" +
		                      scriptW + L"\"";
		if (!argsW.empty()) {
			psArgs += L" ";
			psArgs += argsW;
		}
		return StartProcess(L"powershell.exe", psArgs);
	}

	void TerminalSystem::Stop() {
		if (mRunning) {
			WindowsUtils::CloseProcessHandle(mHandle);
			mRunning = false;
		}
	}

	void TerminalSystem::ClearOutput() {
		mOutputUtf8.clear();
	}

	bool TerminalSystem::StartProcess(
		const std::wstring& app, const std::wstring& args
	) {
		// 実行中なら停止して再実行
		Stop();

		mOutputUtf8.clear();
		mExitCode   = 0;
		mWin32Error = 0;

		mRunning = WindowsUtils::StartProcessCapture(app, args, mHandle);
		if (!mRunning) {
			mWin32Error = GetLastError();
		}
		return mRunning;
	}
}
