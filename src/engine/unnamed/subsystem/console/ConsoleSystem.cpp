#include <pch.h>
//-----------------------------------------------------------------------------
#include <filesystem>
#include <iostream>
#include <ranges>

#include <core/filesystem/Path.h>
#include <core/string/StrUtil.h>
#include <engine/unnamed/subsystem/console/ConsoleSystem.h>
#include <engine/unnamed/subsystem/console/ConsoleUI.h>
#include <engine/unnamed/subsystem/console/ConVarWriter.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <engine/game/GamePathResolver.h>
#include <engine/game/GameRuntimeContext.h>
#include <engine/unnamed/subsystem/console/builtin/BuiltInCommands.h>
#include <engine/unnamed/subsystem/console/builtin/BuiltInConVars.h>
#include <engine/unnamed/subsystem/console/concommand/ConCommand.h>
#include <engine/unnamed/subsystem/console/concommand/ConVar.h>
#include <engine/unnamed/subsystem/console/concommand/base/ConCommandBase.h>
#include <engine/unnamed/subsystem/input/InputSystem.h>
#include <engine/unnamed/subsystem/interface/ServiceLocator.h>
#include <engine/unnamed/subsystem/time/SystemClock.h>

namespace Unnamed {
	static constexpr std::string_view kChannelConsole = "Console";
	static constexpr std::string_view kConsoleLogPath = "./consolesystem.log";

	// 互換性維持用の既存 user.cfg です。
	static constexpr std::string_view kLegacyUserCfgPath =
		"./content/core/cfg/user.cfg";

	[[nodiscard]] int CalculateLevenshteinDistance(
		const std::string_view s1, const std::string_view s2
	) {
		const int len1 = static_cast<int>(s1.length());
		const int len2 = static_cast<int>(s2.length());

		std::vector<std::vector<int>> dp(
			len1 + 1, std::vector<int>(len2 + 1)
		);
		for (int i = 0; i <= len1; ++i) {
			dp[i][0] = i;
		}
		for (int j = 0; j <= len2; ++j) {
			dp[0][j] = j;
		}

		for (int i = 1; i <= len1; ++i) {
			for (int j = 1; j <= len2; ++j) {
				const int cost =
					s1[static_cast<size_t>(i - 1)] ==
					s2[static_cast<size_t>(j - 1)] ?
						0 :
						1;
				dp[i][j] = std::min(
					{
						dp[i - 1][j] + 1,
						dp[i][j - 1] + 1,
						dp[i - 1][j - 1] + cost
					}
				);
			}
		}

		return dp[len1][len2];
	}

	template <typename Range, typename GetNameFn>
	[[nodiscard]] std::vector<std::string> CollectSimilarNames(
		const std::string_view input,
		const size_t           maxResults,
		const Range&           entries,
		GetNameFn              getName
	) {
		std::vector<std::pair<std::string, int>> candidates;
		const std::string lowerInputStr = StrUtil::ToLowerCase(input);

		for (const auto& entry : entries) {
			const std::string_view name      = getName(entry);
			const std::string      lowerName = StrUtil::ToLowerCase(name);
			const int distance = CalculateLevenshteinDistance(
				lowerInputStr, lowerName
			);
			const bool prefixMatch = lowerName.starts_with(lowerInputStr);
			const int threshold = static_cast<int>(lowerInputStr.length()) * 2;

			if (prefixMatch || distance <= threshold) {
				int score = 1000 - distance;
				if (prefixMatch) {
					score += 500;
				}
				candidates.emplace_back(std::string(name), score);
			}
		}

		std::ranges::sort(
			candidates,
			[](const auto& a, const auto& b) {
				return a.second > b.second;
			}
		);

		std::vector<std::string> results;
		results.reserve(std::min(maxResults, candidates.size()));
		for (const auto& [name, _] :
		     candidates | std::views::take(maxResults)) {
			results.push_back(name);
		}

		return results;
	}

	[[nodiscard]] Path ResolvePersistUserCfgPath() {
		const GameRuntimeContext* runtimeContext =
			ServiceLocator::Get<GameRuntimeContext>();
		if (!runtimeContext) {
			return Path(kLegacyUserCfgPath);
		}

		const Path resolvedPath = ResolveGameConfigPath(
			runtimeContext->modulePaths,
			Path("user.cfg")
		);
		if (resolvedPath.IsEmpty()) {
			return Path(kLegacyUserCfgPath);
		}

		const std::filesystem::path configPath = resolvedPath.Native();
		const std::filesystem::path configDir  = configPath.parent_path();
		if (!configDir.empty()) {
			std::error_code ec;
			std::filesystem::create_directories(configDir, ec);
			if (ec) {
				DevMsg(
					kChannelConsole,
					"Failed to create config directory '{}': {}. Falling back to '{}'.",
					Path::ToGenericUtf8(configDir),
					ec.message(),
					Path(kLegacyUserCfgPath)
				);
				return Path(kLegacyUserCfgPath);
			}
		}

		return Path::FromNative(configPath).LexicallyNormal();
	}

	EXEC_FLAG operator|=(EXEC_FLAG& lhs, const EXEC_FLAG& rhs) {
		lhs = static_cast<EXEC_FLAG>(static_cast<int>(lhs) | static_cast<int>(
			                             rhs));
		return lhs;
	}

	EXEC_FLAG operator|(EXEC_FLAG lhs, const EXEC_FLAG& rhs) {
		return lhs |= rhs;
	}

	bool operator&(EXEC_FLAG lhs, EXEC_FLAG rhs) {
		return (static_cast<int>(lhs) & static_cast<int>(rhs)) != 0;
	}

	ConsoleSystem::~ConsoleSystem() {
	};

	bool ConsoleSystem::Init() {
		// サービスロケータに登録
		ServiceLocator::Register<ConsoleSystem>(this);

		// ファイルログの開始
		if (mEnableFileLog) {
			ConsoleFileLogSink::Config cfg;
			cfg.path = Path(kConsoleLogPath);
			if (!mFileLogSink.Start(cfg)) {
				Error(
					kChannelConsole,
					"Failed to start file log sink. File logging will be disabled."
				);
			}
		}

#ifdef UNNAMED_WITH_EDITOR // Editor有効ターゲットではコンソールUIを有効化
		mConsoleUI = std::make_unique<ConsoleUI>(this);
		if (mConsoleUI) {
			mConsoleUI->Init();
		}
#endif

		// 共通コマンドの登録
		RegisterCommonCommands();

		// 内蔵コマンドと変数の登録
		RegisterBuiltInCommands();
		RegisterBuiltInConVars();

		return true;
	}

	void ConsoleSystem::Update(float) {
#ifdef UNNAMED_WITH_EDITOR
		// コンソールUIの更新
		if (mConsoleUI) {
			mConsoleUI->Show();
		}
#endif
	}

	void ConsoleSystem::Shutdown() {
		mHelpCommand.reset();
		mClearCommand.reset();

		// ユーザー設定ファイルに変数を書き込む
		try {
			const Path userCfgPath = ResolvePersistUserCfgPath();
			DevMsg(
				kChannelConsole,
				"Persisting archived convars to '{}'.",
				userCfgPath
			);
			[[maybe_unused]] ConVarWriter writer(userCfgPath);
		} catch (const std::exception& e) {
			Error(
				kChannelConsole,
				"Failed to persist archived convars: {}",
				e.what()
			);
		} catch (...) {
			Error(
				kChannelConsole,
				"Failed to persist archived convars due to unknown exception."
			);
		}

		// ファイルログ停止して残りを書き出し
		mFileLogSink.Stop();

		ServiceLocator::Register<ConsoleSystem>(nullptr);
	}

	const std::string_view ConsoleSystem::GetName() const {
		return "Console";
	}

	RingBuffer<ConsoleLogText, kConsoleBufferSize>&
	ConsoleSystem::GetLogBuffer() {
		return mLogBuffer;
	}

	void ConsoleSystem::Print(
		const LogLevel             level,
		const std::string_view     channel,
		const std::string_view     message,
		const std::source_location location
	) {
#ifndef _DEBUG
		if (level == LogLevel::Dev) {
			// Devレベルのメッセージはリリースビルドでは出力しない
			return;
		}
#endif

		// ログメッセージをバッファに追加
		ConsoleLogText logText;
		logText.level     = level;
		logText.channel   = channel;
		logText.message   = message;
		logText.timeStamp = SystemClock::GetDateTime(SystemClock::Now());
		logText.location  = location;
		mLogBuffer.Push(logText);

		// ファイルへ出力
		if (mEnableFileLog) {
			ConsoleFileLogSink::Event e;
			e.level     = level;
			e.channel   = std::string(channel);
			e.message   = std::string(message);
			e.timeStamp = logText.timeStamp;
			e.location  = location;
			e.threadId  = GetCurrentThreadId();
			mFileLogSink.Enqueue(std::move(e));

			// Error以上は今すぐ書き出す
			if (level >= LogLevel::Error) {
				mFileLogSink.FlushNow(50);
			}
		}

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

#ifdef UNNAMED_WITH_EDITOR
		// UIの更新
		if (mConsoleUI) {
			mConsoleUI->OnConsoleUpdate();
		}
#endif
	}

	void ConsoleSystem::RegisterConCommand(ConCommandBase* conCommand) {
		if (!conCommand) {
			return;
		}
		mConCommands[std::string(conCommand->GetName())] = conCommand;
	}

	void ConsoleSystem::UnregisterConCommand(const ConCommandBase* conCommand) {
		if (!conCommand) {
			return;
		}

		const auto it = mConCommands.find(std::string(conCommand->GetName()));
		if (it != mConCommands.end() && it->second == conCommand) {
			mConCommands.erase(it);
		}
	}

	void ConsoleSystem::RegisterConVar(ConCommandBase* conVar) {
		mConVars[std::string(conVar->GetName())] = conVar;
	}

	template <typename T>
	T ClampConVarValue(
		ConCommandBase* var,
		const T&        value,
		const T&        minValue,
		const T&        maxValue
	) {
		if (value < minValue) {
			Warning(
				kChannelConsole,
				"Value for '{}' is below the minimum ({}). Clamping to minimum.",
				var->GetName(), minValue
			);
			return minValue;
		}
		if (value > maxValue) {
			Warning(
				kChannelConsole,
				"Value for '{}' is above the maximum ({}). Clamping to maximum.",
				var->GetName(), maxValue
			);
			return maxValue;
		}
		return value;
	}

	static std::string GetVarValueString(ConCommandBase* var) {
		if (!var) {
			return {};
		}

		switch (GetConVarType(var)) {
			case CVAR_TYPE::BOOL: {
				if (
					const auto* cBool =
						dynamic_cast<ConVar<bool>*>(var)
				) {
					return cBool->GetValue() ? "true" : "false";
				}
				break;
			}
			case CVAR_TYPE::INT: {
				if (
					const auto* ci = dynamic_cast<ConVar<int>*>(var)
				) {
					return std::to_string(ci->GetValue());
				}
				break;
			}
			case CVAR_TYPE::FLOAT: {
				if (
					const auto* cf =
						dynamic_cast<ConVar<float>*>(var)
				) {
					return std::to_string(cf->GetValue());
				}
				break;
			}
			case CVAR_TYPE::DOUBLE: {
				if (
					const auto* cd = dynamic_cast<ConVar<double>*>(
						var)
				) {
					return std::to_string(cd->GetValue());
				}
				break;
			}
			case CVAR_TYPE::STRING: {
				if (
					const auto* cs =
						dynamic_cast<ConVar<std::string>*>(var)
				) {
					return cs->GetValue();
				}
				break;
			}
			case CVAR_TYPE::VEC3: {
				if (
					const auto* cv3 =
						dynamic_cast<ConVar<Vec3>*>(var)
				) {
					return cv3->GetValue().ToString();
				}
				break;
			}
			case CVAR_TYPE::NONE:
			default: break;
		}
		return {};
	}

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

		// 余分な空白を除去
		std::string trimmed = StrUtil::TrimSpaces(std::string(command));

		// コマンドを区切る
		const auto commands = StrUtil::SplitCommands(trimmed);

		if (flag & EXEC_FLAG::SILENT) {
			// サイレントフラグが指定されている場合は表示しない
		} else {
			// 送信内容をコンソールに表示
			SpecialMsg(
				LogLevel::Execute,
				kChannelConsole,
				"> {}",
				trimmed
			);
#ifdef UNNAMED_WITH_EDITOR
			// UIに通知
			if (mConsoleUI) {
				mConsoleUI->OnConsoleUpdate();
			}
#endif
		}

		// コマンドを順に処理
		for (const auto& singleCommand : commands) {
			// 空のコマンドは無視
			if (singleCommand.empty()) {
				continue;
			}

			// コマンドをトークンに分割
			std::vector<std::string> tokens = StrUtil::Tokenize(singleCommand);

			// トークンがない場合は無視
			if (tokens.empty()) {
				continue;
			}

			// 最初がコマンドで、残りが引数
			std::vector args(tokens.begin() + 1, tokens.end());

			// ダブルクォートがある場合は取り除いておく
			for (auto& arg : args) {
				if (arg.find('"') == std::string::npos) {
					continue;
				}
				// 引数から両端のダブルクォートを削除
				arg = StrUtil::RemoveDoubleQuotes(arg);
			}

			// コマンドと変数の両方を検索しておく
			const bool foundCommand = mConCommands.contains(tokens[0]);
			const bool foundVar     = mConVars.contains(tokens[0]);

			// アクションの処理
			if (!foundCommand && !foundVar && !tokens[0].empty()) {
				const char prefix = tokens[0][0];
				if (prefix == '+' || prefix == '-') {
					const std::string actionName = tokens[0].substr(1);
					if (!actionName.empty()) {
						if (auto* input = ServiceLocator::Get<InputSystem>()) {
							input->HandleConsoleAction(
								actionName,
								prefix == '+'
							);
							break;
						}
					}
				}
			}

			// コマンドの場合
			if (foundCommand) {
				auto* base = mConCommands[tokens[0]];
				if (!base || !base->IsCommand()) {
					SpecialMsg(
						LogLevel::Error,
						"",
						"'{}' is not recognized as a command.",
						tokens[0]
					);
					break;
				}

				const auto* cmd = dynamic_cast<ConCommand*>(base);

				if (cmd->HasFlags(FCVAR::NOTIFY)) {
					ExecuteCommand(
						std::format(
							"notify info 5 Command '{}' executed.",
							cmd->GetName()
						),
						EXEC_FLAG::SILENT

					);
				}

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

				break;
			}

			// 変数の場合
			if (foundVar) {
				auto* var = mConVars[tokens[0]];

				// 引数がある場合は値を設定 / 無い場合は表示
				if (!args.empty()) {
					SetVarFromArgs(var, args);
				} else {
					const auto        cvarType = GetConVarType(var);
					const std::string type     = ToString(cvarType);
					const std::string value    = GetVarValueString(var);
					SpecialMsg(
						LogLevel::Info,
						"",
						"{} = [{}] {} : {}",
						var->GetName(),
						type,
						value,
						var->GetDescription()
					);
				}
			} else {
				// Silentフラグがない場合はエラーを表示
				if (!(flag & EXEC_FLAG::SILENT)) {
					// 謎な場合
					SpecialMsg(
						LogLevel::Error,
						"",
						"'{}' is not recognized as a command or variable.",
						tokens[0]
					);
				}
			}
		}
	}

	std::unordered_map<std::string, ConCommandBase*>
	ConsoleSystem::GetConVars() {
		return mConVars;
	}

	ConCommandBase* ConsoleSystem::GetConVar(
		const std::string_view name
	) {
		const auto it = mConVars.find(std::string(name));
		return it != mConVars.end() ? it->second : nullptr;
	}

	ConCommandBase* ConsoleSystem::GetConCommand(
		const std::string_view name
	) {
		const auto it = mConCommands.find(std::string(name));
		return it != mConCommands.end() ? it->second : nullptr;
	}

	std::string ConsoleSystem::GetConVarValueString(
		const std::string_view name
	) const {
		const auto it = mConVars.find(std::string(name));
		if (it == mConVars.end()) {
			return {};
		}
		return GetVarValueString(it->second);
	}

	CVAR_TYPE ConsoleSystem::GetConVarType(ConCommandBase* var) {
		auto type = CVAR_TYPE::NONE;

		if (dynamic_cast<ConVar<bool>*>(
			var)) {
			type = CVAR_TYPE::BOOL;
		} else if (dynamic_cast<
			ConVar<int>*>(var)) {
			type = CVAR_TYPE::INT;
		} else if (
			dynamic_cast<ConVar<float>*>(
				var)) {
			type = CVAR_TYPE::FLOAT;
		} else if (dynamic_cast<
			ConVar<double>*>(var)) {
			type = CVAR_TYPE::DOUBLE;
		} else if
		(dynamic_cast<ConVar<std::string>*>(var)) {
			type = CVAR_TYPE::STRING;
		} else if (dynamic_cast<ConVar<Vec3>*>(var)) {
			type = CVAR_TYPE::VEC3;
		}

		return type;
	}

	template <typename T>
	T ConsoleSystem::GetConVarValueOr(
		const std::string_view name,
		const T&               fallback
	) {
		if (const auto* var = GetConVarAs<ConVar<T>>(name)) {
			return var->GetValue();
		}
		Print(
			LogLevel::Warning, "Console",
			std::format(
				"CVar '{}' not found. Returning fallback value.", name
			),
			std::source_location::current()
		);
		return fallback;
	}

	template bool ConsoleSystem::GetConVarValueOr<bool>(
		std::string_view,
		const bool&
	);
	template int ConsoleSystem::GetConVarValueOr<int>(
		std::string_view,
		const int&
	);
	template float ConsoleSystem::GetConVarValueOr<float>(
		std::string_view,
		const float&
	);

	std::vector<std::string> ConsoleSystem::FindSimilarConVars(
		const std::string_view input, size_t maxResults
	) {
		return CollectSimilarNames(
			input,
			maxResults,
			mConVars,
			[](const auto& entry) -> std::string_view {
				return entry.first;
			}
		);
	}

	std::vector<std::string> ConsoleSystem::FindSimilarConCommands(
		const std::string_view input, size_t maxResults
	) {
		return CollectSimilarNames(
			input,
			maxResults,
			mConCommands,
			[](const auto& entry) -> std::string_view {
				return entry.first;
			}
		);
	}

	void ConsoleSystem::RegisterCommonCommands() {
		mHelpCommand = std::make_unique<ConCommand>(
			"help",
			[&](const std::vector<std::string>& args) {
				// 引数がある場合はそのコマンド・変数の説明を表示する
				if (!args.empty()) {
					SpecialMsg(
						LogLevel::Info,
						"",
						"{} : {}",
						args[0],
						mConCommands.contains(args[0]) ?
						mConCommands[args[0]]->GetDescription() :
						mConVars.contains(args[0]) ?
						mConVars[args[0]]->GetDescription() :
						"Command or variable not found."
					);
				} else {
					// 引数が無い場合は全てのコマンド・変数の一覧を表示する
					for (
						const auto& command : mConCommands | std::views::values
					) {
						SpecialMsg(
							LogLevel::Info,
							kChannelNone,
							"{} : {}",
							command->GetName(),
							command->GetDescription()
						);
					}
					for (const auto& var : mConVars | std::views::values) {
						auto level = LogLevel::Info;

						switch (GetConVarType(var)) {
							case CVAR_TYPE::BOOL: level = LogLevel::Bool;
								break;
							case CVAR_TYPE::INT: level = LogLevel::Int;
								break;
							case CVAR_TYPE::FLOAT: level = LogLevel::Float;
								break;
							case CVAR_TYPE::DOUBLE: level = LogLevel::Double;
								break;
							case CVAR_TYPE::STRING: level = LogLevel::String;
								break;
							case CVAR_TYPE::VEC3: level = LogLevel::Vec3;
								break;
							case CVAR_TYPE::NONE:
							default: level = LogLevel::Info;
								break;
						}

						SpecialMsg(
							level,
							kChannelNone,
							"{} : {}",
							var->GetName(),
							var->GetDescription()
						);
					}
				}
				return true;
			},
			"Display help information for commands and variables."
		);

		mClearCommand = std::make_unique<ConCommand>(
			"clear",
			[&](const std::vector<std::string>&) {
				GetLogBuffer().Clear();
				return true;
			},
			"Clears the console output."
		);
	}

	void ConsoleSystem::SetVarFromArgs(
		ConCommandBase* var, const std::vector<std::string>& args
	) {
		// 変数が存在しない、または引数が空の場合は何もしない
		if (!var || args.empty()) {
			return;
		}

		switch (GetConVarType(var)) {
			case CVAR_TYPE::BOOL: {
				if (
					auto* cBool = dynamic_cast<ConVar<bool>*>(var)
				) {
					const bool newValue = StrUtil::CheckBoolString(args[0]);

					if (cBool->HasFlags(FCVAR::NOTIFY)) {
						ExecuteCommand(
							std::format(
								"notify info 5 ConVar '{}' changed | '{}' → '{}'",
								cBool->GetName(),
								cBool->GetValue() ? "true" : "false",
								newValue ?
									"true" :
									"false"
							),
							EXEC_FLAG::SILENT
						);
					}

					cBool->SetValue(newValue);
				}
				break;
			}
			case CVAR_TYPE::INT: {
				if (
					auto* ci = dynamic_cast<ConVar<int>*>(var)
				) {
					int value;
					try {
						value = std::stoi(args[0]);
					} catch (const std::exception& e) {
						SpecialMsg(
							LogLevel::Error, "",
							"Invalid integer value: '{}'. {}",
							args[0], e.what()
						);
						return;
					}
					// 最小値または最大値が設定されている場合はクランプする
					if (ci->HasMinValue() || ci->HasMaxValue()) {
						value = ClampConVarValue(
							ci,
							value,
							ci->GetMinValue(),
							ci->GetMaxValue()
						);
					}

					if (ci->HasFlags(FCVAR::NOTIFY)) {
						ExecuteCommand(
							std::format(
								"notify info 5 ConVar '{}' changed | '{}' → '{}'",
								ci->GetName(),
								ci->GetValue(),
								value
							),
							EXEC_FLAG::SILENT
						);
					}

					ci->SetValue(value);
				}
				break;
			}
			case CVAR_TYPE::FLOAT: {
				if (
					auto* cf = dynamic_cast<ConVar<float>*>(var)
				) {
					float value;
					try {
						value = std::stof(args[0]);
					} catch (const std::exception& e) {
						SpecialMsg(
							LogLevel::Error, "",
							"Invalid float value: '{}'. {}",
							args[0], e.what()
						);
						return;
					}
					// 最小値または最大値が設定されている場合はクランプする
					if (cf->HasMinValue() || cf->HasMaxValue()) {
						value = ClampConVarValue(
							cf,
							value,
							cf->GetMinValue(),
							cf->GetMaxValue()
						);
					}

					if (cf->HasFlags(FCVAR::NOTIFY)) {
						ExecuteCommand(
							std::format(
								"notify info 5 ConVar '{}' changed | '{}' → '{}'",
								cf->GetName(),
								cf->GetValue(),
								value
							),
							EXEC_FLAG::SILENT
						);
					}

					cf->SetValue(value);
				}
				break;
			}
			case CVAR_TYPE::DOUBLE: {
				if (
					auto* cd = dynamic_cast<ConVar<double>*>(var)
				) {
					double value;
					try {
						value = std::stof(args[0]);
					} catch (std::exception& e) {
						SpecialMsg(
							LogLevel::Error, "",
							"Invalid double value: '{}'. {}",
							args[0], e.what()
						);
						return;
					}
					// 最小値または最大値が設定されている場合はクランプする
					if (cd->HasMinValue() || cd->HasMaxValue()) {
						value = ClampConVarValue(
							cd,
							value,
							cd->GetMinValue(),
							cd->GetMaxValue()
						);
					}

					if (cd->HasFlags(FCVAR::NOTIFY)) {
						ExecuteCommand(
							std::format(
								"notify info 5 ConVar '{}' changed | '{}' → '{}'",
								cd->GetName(),
								cd->GetValue(),
								value
							),
							EXEC_FLAG::SILENT
						);
					}

					cd->SetValue(value);
				}
				break;
			}
			case CVAR_TYPE::STRING: {
				if (
					auto* cs =
						dynamic_cast<ConVar<std::string>*>(var)
				) {
					std::string combined;
					for (size_t i = 0; i < args.size(); ++i) {
						combined += args[i];
						if (i + 1 < args.size()) {
							combined += " ";
						}
					}

					if (cs->HasFlags(FCVAR::NOTIFY)) {
						ExecuteCommand(
							std::format(
								"notify info 5 ConVar '{}' changed | '{}' → '{}'",
								cs->GetName(),
								cs->GetValue(),
								combined
							),
							EXEC_FLAG::SILENT
						);
					}

					cs->SetValue(combined);
				}
				break;
			}
			case CVAR_TYPE::VEC3: {
				if (args.size() >= 3) {
					if (
						auto* cv3 = dynamic_cast<ConVar<Vec3>*>(var)
					) {
						bool ok = true;
						for (const auto& arg : args | std::views::take(3)) {
							ok &= StrUtil::IsFloat(arg);
						}
						if (ok) {
							cv3->SetValue(
								Vec3(
									std::stof(args[0]),
									std::stof(args[1]),
									std::stof(args[2])
								)
							);
						}
					}
				}
				break;
			}
			case CVAR_TYPE::NONE:
			default: {
				SpecialMsg(
					LogLevel::Error, "", "Unknown variable type for '{}'.",
					var->GetName()
				);
				break;
			}
		}
	}
}
