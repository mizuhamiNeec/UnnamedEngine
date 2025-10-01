#include <pch.h>
//-----------------------------------------------------------------------------
#include <iostream>

#include <engine/OldConsole/Console.h>
#include <engine/subsystem/console/ConsoleSystem.h>
#include <engine/subsystem/console/Log.h>
#include <engine/subsystem/console/concommand/base/UnnamedConCommandBase.h>
#include <engine/subsystem/console/concommand/base/UnnamedConVarBase.h>
#include <engine/subsystem/interface/ServiceLocator.h>
#include <engine/subsystem/time/SystemClock.h>

#include "engine/subsystem/console/concommand/UnnamedConCommand.h"
#include "engine/subsystem/console/concommand/UnnamedConVar.h"

namespace Unnamed {
	static constexpr std::string_view kChannel = "Console";

	EXEC_FLAG operator|=(EXEC_FLAG& lhs, const EXEC_FLAG& rhs) {
		lhs = static_cast<EXEC_FLAG>(static_cast<int>(lhs) | static_cast<int>(
			rhs));
		return lhs;
	}

	bool operator&(EXEC_FLAG lhs, EXEC_FLAG rhs) {
		return (static_cast<int>(lhs) & static_cast<int>(rhs)) != 0;
	}

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

		std::string out;
		if (!logText.channel.empty()) {
			out = "[" + logText.channel + "] " + logText.message;
		} else {
			out = logText.message;
		}

		// コンソールの出力
		std::cout << out << "\n";

		// Visual Studioの出力
		OutputDebugStringW(StrUtil::ToWString(out).c_str());
		OutputDebugStringW(StrUtil::ToWString("\n").c_str());
	}

	void ConsoleSystem::RegisterConCommand(UnnamedConCommandBase* conCommand) {
		mConCommands[std::string(conCommand->GetName())] = conCommand;

		DevMsg(
			kChannel,
			"コマンドが登録されました: {}",
			conCommand->GetName()
		);
	}

	void ConsoleSystem::RegisterConVar(UnnamedConVarBase* conVar) {
		mConVars[std::string(conVar->GetName())] = conVar;

		DevMsg(
			kChannel,
			"ConVarが登録されました"
		);
	}

	/// @brief コンソールにコマンドを送信します。
	/// @param command コマンド文字列
	/// @param flag フラグ
	void ConsoleSystem::ExecuteCommand(
		const std::string& command,
		const EXEC_FLAG    flag
	) {
		// 空なので何もしない
		if (command.empty()) {
			SpecialMsg(
				LogLevel::Execute,
				"",
				" ]"
			);
			return;
		}

		std::string trimmed = TrimSpaces(std::string(command));

		// コマンドを区切る
		auto commands = SplitCommands(trimmed);

		if (flag & EXEC_FLAG::SILENT) {
			// TODO 
		} else {
			// 送信内容をコンソールに表示
			SpecialMsg(
				LogLevel::Execute,
				"",
				"> {}",
				trimmed
			);
		}

		for (const auto& singleCommand : commands) {
			if (singleCommand.empty()) {
				continue;
			}

			std::vector<std::string> tokens = Tokenize(singleCommand);
			const std::vector        args(tokens.begin() + 1, tokens.end());

			const bool foundCommand = mConCommands.contains(tokens[0]);
			const bool foundVar     = mConVars.contains(tokens[0]);

			// コマンドの場合
			if (foundCommand) {
				const auto* cmd = reinterpret_cast<UnnamedConCommand*>(
					mConCommands[tokens[0]]
				);

				// コールバックを実行
				if (cmd->onExecute(args)) {
					// 実行が完了したら完了時のコールバックを呼ぶ
					if (cmd->onComplete) {
						cmd->onComplete();
					}
				} else {
					// 失敗したらとりあえず説明を出しておく
					SpecialMsg(
						LogLevel::None,
						"",
						"{}: {}",
						cmd->GetName(),
						cmd->GetDescription()
					);
				}
			}

			// 変数の場合
			if (foundVar) {
			}
		}
	}

	void ConsoleSystem::Test() {
		for (auto conVar : mConVars) {
			DevMsg(kChannel, "ConVar: {}", conVar.first);

			if (auto* cbool = dynamic_cast<UnnamedConVar<bool>*>(conVar.
				second)) {
				DevMsg(kChannel, "Value: {}", static_cast<bool>(*cbool));
			} else if (auto* cint = dynamic_cast<UnnamedConVar<int>*>(
				conVar.second)) {
				DevMsg(kChannel, "Value: {}", static_cast<int>(*cint));
			} else if (auto* cfloat = dynamic_cast<UnnamedConVar<float>*>(conVar
				.second)) {
				DevMsg(kChannel, "Value: {}", static_cast<float>(*cfloat));
			} else if (auto* convarDouble = dynamic_cast<UnnamedConVar<double>*>
				(conVar.second)) {
				DevMsg(kChannel, "Value: {}",
				       static_cast<double>(*convarDouble));
			} else if (auto* convarString = dynamic_cast<UnnamedConVar<
				std::string>*>(conVar.second)) {
				DevMsg(kChannel, "Value: {}",
				       static_cast<std::string>(*convarString));
			}
		}
	}

	std::vector<std::string> ConsoleSystem::SplitCommands(
		const std::string_view& command
	) {
		std::vector<std::string> result;
		std::string              current;
		bool                     inQuotes = false;
		for (const char ch : command) {
			if (ch == '"') {
				inQuotes = !inQuotes;
				current += ch;
			} else if (ch == ';' && !inQuotes) {
				result.emplace_back(current);
				current.clear();
			} else {
				current += ch;
			}
		}

		if (!current.empty()) {
			result.emplace_back(current);
		}

		return result;
	}

	std::vector<std::string> ConsoleSystem::Tokenize(
		const std::string_view& command
	) {
		std::istringstream       stream{std::string(command)};
		std::vector<std::string> tokens;
		std::string              token;

		while (stream >> token) {
			tokens.emplace_back(token);
		}

		return tokens;
	}

	std::string ConsoleSystem::TrimSpaces(const std::string& string) {
		const size_t start = string.find_first_not_of(" \t\n\r");
		const size_t end   = string.find_last_not_of(" \t\n\r");

		if (start == std::string::npos || end == std::string::npos) {
			return "";
		}

		return string.substr(start, end - start + 1);
	}
}
