#include <pch.h>

//-----------------------------------------------------------------------------

#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>

#include <core/unnamed/ini/IniParser.h>
#include <engine/ImGui/Icons.h>
#include <engine/ImGui/ImGuiManager.h>
#include <engine/ImGui/ImGuiUtil.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConCommand.h>
#include <engine/OldConsole/Console.h>
#include <engine/OldConsole/ConVarManager.h>
#include <engine/unnamed/subsystem/time/SystemClock.h>
#include <engine/Window/WindowsUtils.h>

#include "engine/ImGui/ImGuiWidgets.h"

using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);

/// @brief コンソールクラスのコンストラクタ
Console::Console() {
	mStopThread = false;

#ifdef _DEBUG
	// ログファイルの初期化
	{
		std::lock_guard lock(mMutex);

		// 既存のログファイルを削除
		if (std::filesystem::exists("console.log")) {
			std::filesystem::remove("console.log");
		}

		// 新規ファイルを作成
		mLogFile.open("console.log", std::ios::out | std::ios::binary);
		if (mLogFile.is_open()) {
			// ヘッダーを書き込む
			const auto now = SystemClock::GetDateTime(SystemClock::StartTime());
			std::string header = std::format(
				"//-----------------------------------------------------------------------------\n"
				"// BuildDate: {}-{}\n"
				"// Engine: {} Ver. {}\n"
				"// LaunchDate: {:02}-{:02}-{:02} {:02}:{:02}:{:02}\n"
				"//-----------------------------------------------------------------------------\n\n",
				__DATE__, __TIME__,
				ENGINE_NAME, ENGINE_VERSION,
				now.year, now.month, now.day, now.hour, now.minute, now.second
			);
			mLogFile.write(header.c_str(), header.size());
			mLogFile.flush();
		}
	}
#endif
	StartConsoleThread();

	ConCommand::Init();

	ConCommand::RegisterCommand(
		"toggleconsole", ToggleConsole,
		"Show/hide the console."
	);

	ConCommand::RegisterCommand("clear", Clear, "Clear all console output");
	ConCommand::RegisterCommand("cls", Clear, "Clear all console output");
	ConCommand::RegisterCommand("echo", Echo, "Echo text to console.");
	ConCommand::RegisterCommand(
		"help", Help,
		"Find help about a convar/concommand."
	);
	ConCommand::RegisterCommand("neofetch", NeoFetch, "Show system info.");
	SubmitCommand("bind ` toggleconsole", true);
}

/// @brief コンソールクラスのデストラクタ
Console::~Console() { Shutdown(); }

/// @brief コンソールを更新します
void Console::Update() {
#ifdef _DEBUG
	mFrameCount++;
	if (!mShowConsole) { return; }

	// ウィンドウフラグの設定
	ImGuiWindowFlags consoleWindowFlags =
		ImGuiWindowFlags_NoScrollbar |       // スクロールバーを非表示
		ImGuiWindowFlags_NoScrollWithMouse | // マウスホイールでスクロールしない
		ImGuiWindowFlags_NoCollapse |        // ウィンドウの折り畳みを無効
		ImGuiWindowFlags_MenuBar;            // メニューバーを表示

	if (mShowSuggestPopup) {
		// フォーカス時に前面に持ってこない
		consoleWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	// 最小サイズと最大サイズを設定
	ImGui::SetNextWindowSizeConstraints(
		{360.0f, 360.0f},
		{0xFFFF, 0xFFFF}
	);

	bool bWindowOpen = ImGui::Begin(
		(Unnamed::StrUtil::ConvertToUtf8(kIconTerminal) + " コンソール").c_str(),
		&mShowConsole, consoleWindowFlags
	);

	if (bWindowOpen) {
		ShowMenuBar();

		mFocusedConsoleWindow = ImGui::IsWindowFocused(
			ImGuiFocusedFlags_RootAndChildWindows
		);

		if (!WindowsUtils::IsAppDarkTheme()) {
			if (mFocusedConsoleWindow) {
				ImGui::PushStyleColor(
					ImGuiCol_ChildBg,
					ImGuiUtil::ToImVec4(kConBgColorLight)
				);
			} else {
				Vec4 color = kConBgColorLight;
				color.w    = 0.125f;
				ImGui::PushStyleColor(
					ImGuiCol_ChildBg,
					ImGuiUtil::ToImVec4(color)
				);
				ImGui::PushStyleColor(
					ImGuiCol_WindowBg,
					ImGuiUtil::ToImVec4(color)
				);
			}
		} else {
			if (mFocusedConsoleWindow) {
				ImGui::PushStyleColor(
					ImGuiCol_ChildBg,
					ImGuiUtil::ToImVec4(kConBgColorDark)
				);
			} else {
				Vec4 color = kConBgColorDark;
				color.w    = 0.125f;
				ImGui::PushStyleColor(
					ImGuiCol_ChildBg,
					ImGuiUtil::ToImVec4(color)
				);
				ImGui::PushStyleColor(
					ImGuiCol_WindowBg,
					ImGuiUtil::ToImVec4(color)
				);
			}
		}

		if (
			ImGuiWidgets::IconButton(
				Unnamed::StrUtil::ConvertToUtf8(kIconTerminal).c_str(),
				"ConVar",
				{48.0f, 48.0f},
				0.5f
			)
		) { mShowConVarHelper = !mShowConVarHelper; }

		ImGui::Spacing();
		ImGui::Spacing();

		ShowConsoleBody();
		if (mFocusedConsoleWindow) { ImGui::PopStyleColor(); } else {
			ImGui::PopStyleColor(2);
		}

		if (mShowAbout) { ShowAbout(); }
	}
	ImGui::End();

	ShowConVarHelper();
#endif
}

/// @brief コンソールをシャットダウンします
void Console::Shutdown() {
	{
		std::lock_guard lock(mMutex);
		mStopThread = true;
	}

	// スレッドを再開
	mCv.notify_all();

	// スレッドが終了するまで待機
	if (mConsoleThread.joinable()) { mConsoleThread.join(); }

#ifdef _DEBUG
	// 残りのメッセージをファイルに書き込む
	if (!mMessageBuffer.empty()) {
		FlushLogBuffer(mMessageBuffer);
		mMessageBuffer.clear();
	}

	// ログファイルを閉じる
	if (mLogFile.is_open()) { mLogFile.close(); }
#endif
}

/// @brief コマンドを送信/実行します
/// @param command コマンド文字列
/// @param bSilent コマンドを実行した際にログに出力しないかどうか
void Console::SubmitCommand(
	[[maybe_unused]] const std::string& command,
	const bool                          bSilent
) {
	std::string trimmedCommand = TrimSpaces(command);

	// コマンドが空なのでなんもしない
	if (trimmedCommand.empty()) {
		Print(">\n", kConFgColorDark);
		return;
	}

	// セミコロンでコマンドを区切る
	std::vector<std::string> commands = SplitCommands(trimmedCommand);

	if (bSilent) { AddCommandHistory(trimmedCommand); } else {
		Print(
			"> " + trimmedCommand + "\n",
			kConTextColorExecute,
			Channel::Console
		);
	}

	for (const auto& singleCommand : commands) {
		std::string cmd = TrimSpaces(singleCommand);

		if (cmd.empty()) { continue; }

		std::vector<std::string> tokens = TokenizeCommand(cmd);

		bool found = ConCommand::ExecuteCommand(cmd);

		// InputSystemのコマンド実行
		// アクションコマンドの処理
		if (!found && !tokens.empty()) {
			// +-プレフィックスの処理
			if (tokens[0][0] == '+' || tokens[0][0] == '-') {
				bool isDown = (tokens[0][0] == '+');
				InputSystem::ExecuteCommand(tokens[0], isDown);
				found = true;
			}
		}

		for (auto conVar : ConVarManager::GetAllConVars()) {
			// 変数が存在する場合
			if (Unnamed::StrUtil::Equal(conVar->GetName(), tokens[0])) {
				found = true;
				// 変数のみ入力された場合
				if (tokens.size() < 2) {
					// 現在の変数の値を表示
					Print(
						std::format(
							R"("{}" = "{}")",
							conVar->GetName(),
							conVar->GetValueAsString()
						) + "\n",
						kConTextColorWarning
					);
					// 変数の説明を取得
					std::string description = conVar->GetHelp();
					// 変数の型を取得
					std::string type = std::format(
						"[{}]\n", conVar->GetTypeAsString()
					);

					Print(
						" - " + description + " " + type,
						GetConVarTypeColor(conVar->GetTypeAsString())
					);
				} else {
					// 引数込みで入力された場合の処理
					bool isValidInput = true;
					// Vec3の場合の処理
					if (conVar->GetTypeAsString() == "vec3") {
						// 引数が3つあることを確認
						if (tokens.size() < 4) {
							// tokens[0]が変数名なので、1,2,3の引数を含めて最低4つ必要
							isValidInput = false;
						} else {
							try {
								// 1, 2, 3の順番でx, y, zを取得
								float x = std::stof(tokens[1]);
								float y = std::stof(tokens[2]);
								float z = std::stof(tokens[3]);

								// Vec3型の文字列として値を設定
								std::string vec3Value = std::format(
									"{} {} {}", x, y, z
								);
								conVar->SetValueFromString(vec3Value);
								// ここで値を設定
							} catch (...) { isValidInput = false; }
						}
					} else {
						for (size_t i = 1; i < tokens.size(); ++i) {
							if (conVar->GetTypeAsString() == "int") {
								if (tokens[i] == "true") {
									tokens[i] = "1";
								} else if (tokens[i] == "false") {
									tokens[i] = "0";
								}

								try {
									[[maybe_unused]] int value = std::stoi(
										tokens[i]
									);
								} catch (...) {
									isValidInput = false;
									break;
								}
							} else if (conVar->GetTypeAsString() == "float") {
								try {
									[[maybe_unused]] float value = std::stof(
										tokens[i]
									);
								} catch (...) {
									isValidInput = false;
									break;
								}
							} else if (conVar->GetTypeAsString() == "bool") {
								try {
									// 入力が数値として解釈できる場合
									int value = std::stoi(tokens[i]);
									tokens[i] = (value == 0) ? "false" : "true";
								} catch (...) {
									// 数値に変換できなかった場合
									if (tokens[i] == "true" || tokens[i] ==
									    "false") {
										// 入力が "true" または "false" の場合はそのまま
										continue;
									}
									isValidInput = false;
									break;
								}
							}
						}
					}
					if (isValidInput) {
						// Vec3型の場合は個別の処理を行ったのでスキップ
						if (conVar->GetTypeAsString() != "vec3") {
							for (size_t i = 1; i < tokens.size(); ++i) {
								conVar->SetValueFromString(tokens[i]);
							}
						}
					} else {
						Print(
							"CVAR型変換エラー: 指定された型が無効です。\n", kConTextColorError,
							Channel::Console
						);
						Print(
							"期待される型: " + conVar->GetTypeAsString() + "\n",
							GetConVarTypeColor(conVar->GetTypeAsString()),
							Channel::Console
						);
					}
				}
				break;
			}
		}

		// コマンドが見つからなかった
		if (!found) {
			Print(std::format("Unknown command: {}\n", trimmedCommand));
		}
	}

#ifdef _DEBUG
	mWishScrollToBottom = true;
#endif
}

/// @brief コンソールにテキストを出力します
/// @param message 出力するテキスト
/// @param color テキストの色
/// @param channel チャンネル
void Console::Print(
	[[maybe_unused]] const std::string& message,
	[[maybe_unused]] const Vec4&        color,
	[[maybe_unused]] const Channel&     channel
) {
	if (message.empty()) { return; }

	// ログへの書き込み
	if (ConVarManager::GetConVar("verbose")->GetValueAsBool()) {
		std::string channelStr = (channel != Channel::None) ?
			                         "[" + ToString(channel) + "] " :
			                         "";
		OutputDebugString(
			Unnamed::StrUtil::ToWString(channelStr + message).c_str()
		);
		LogToFileAsync(channelStr + message);
	}

	// タスクキューに追加
	{
		std::lock_guard lock(mMutex);
		mTaskQueue.emplace(
			[message, color, channel] {
				std::lock_guard lock(mMutex);

				std::string msg = message;
				const bool hasNewLine = !msg.empty() && msg.back() == '\n';
				const std::string baseMsg = hasNewLine ?
					                            msg.substr(0, msg.size() - 1) :
					                            msg;

#ifdef _DEBUG
				if (!mConsoleTexts.empty()) {
					const std::string lastMsg = mConsoleTexts.back().text;
					const bool        lastHasNewLine =
						!lastMsg.empty() && lastMsg.back()
						== '\n';
					const std::string lastBaseMsg = lastHasNewLine ?
						                                lastMsg.substr(
							                                0, lastMsg.size() -
								                                1
						                                ) :
						                                lastMsg;

					const size_t      bracketPos = lastBaseMsg.find(" [x");
					const std::string lastBaseMsgWithoutCount =
						(bracketPos != std::string::npos) ?
							lastBaseMsg.substr(0, bracketPos) :
							lastBaseMsg;

					if (baseMsg == lastBaseMsgWithoutCount && lastMsg !=
					    "]\n") {
						UpdateRepeatCount(baseMsg, hasNewLine, channel, color);
						return;
					}
				}

				mConsoleTexts.emplace_back(Text(msg, color, channel));
				mRepeatCounts.emplace_back(1);
				mWishScrollToBottom = true;
#endif
			}
		);
	}
	mCv.notify_one();
}

/// @brief 開発者を叱ります
/// @param message メッセージ
/// @param channel チャンネル
void Console::PrintNullptr(const std::string& message, const Channel& channel) {
	Print("ぬるぽ >> " + message + "\n", kConTextColorError, channel);
	Print("ｶﾞｯ\n", kConTextColorWarning, channel);
}

/// @brief チャンネルを文字列に変換します
/// @param channel チャンネル
/// @return 文字列に変換されたチャンネル
std::string Console::ToString(const Channel channel) {
	switch (channel) {
		case Channel::None: return "";
		case Channel::CommandLine: return "CommandLine";
		case Channel::Console: return "Console";
		case Channel::Engine: return "Engine";
		case Channel::Host: return "Host";
		case Channel::AssetSystem: return "AssetSystem";
		case Channel::ResourceSystem: return "ResourceSystem";
		case Channel::InputSystem: return "InputSystem";
		case Channel::Client: return "Client";
		case Channel::Server: return "Server";
		case Channel::Game: return "Game";
		case Channel::Physics: return "Physics";
		case Channel::RenderPipeline: return "RenderPipeline";
		case Channel::RenderSystem: return "RenderSystem";
		case Channel::UserInterface: return "UserInterface";
		case Channel::Sound: return "Sound";
		case Channel::General: return "General";
		case Channel::Developer: return "Developer";
	}
	return "unknown";
}

/// @brief コンソールの表示/非表示を切り替えます
/// @param args コマンド引数
void Console::ToggleConsole(
	[[maybe_unused]] const std::vector<std::string>& args
) {
#ifdef _DEBUG
	mShowConsole = !mShowConsole;
#endif
}

/// @brief コンソールのテキストをクリアします
/// @param args コマンド引数
void Console::Clear([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	mConsoleTexts.clear();         // コンソールのテキストをクリア
	mConsoleTexts.shrink_to_fit(); // 開放
	mHistory.clear();              // コマンド履歴をクリア
	mHistory.shrink_to_fit();
	mSuggestions.clear(); // サジェストをクリア
	mSuggestions.shrink_to_fit();
	mRepeatCounts.clear(); // 繰り返しカウントをクリア
	mRepeatCounts.shrink_to_fit();
	mHistoryIndex       = -1;   // 履歴インデックスを初期化
	mWishScrollToBottom = true; // 再描画の際にスクロールをリセット
#endif
}

/// @brief ヘルプを表示します
/// @param args コマンド引数
void Console::Help([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	ConCommand::Help();
	for (auto conVar : ConVarManager::GetAllConVars()) {
		Print(
			" - " + conVar->GetName() + " : " + conVar->GetHelp() + "\n",
			kConFgColorDark, Channel::None
		);
	}
#endif
}

/// @brief システム情報を表示します
/// @param args コマンド引数
void Console::NeoFetch([[maybe_unused]] const std::vector<std::string>& args) {
	std::vector<std::string> asciiArt = {
		"                   <<<<<<<<                  ",
		"                 <<<<<<<<                    ",
		"              <<<<<<<<                       ",
		"            <<<<<<<<                         ",
		"         <<<<<<<<                            ",
		"       <<<<<<<<                              ",
		"    <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  ",
		"  <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  ",
		"    <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<  ",
		"       <<<<<<<<                              ",
		"         <<<<<<<<                            ",
		"            <<<<<<<<                         ",
		"              <<<<<<<<                       ",
		"                 <<<<<<<<                    ",
		"                   <<<<<<<<                  ",
		"                                             "
	};

	const std::string prompt = WindowsUtils::GetWindowsUserName() + "@" +
	                           WindowsUtils::GetWindowsComputerName();

	const DateTime dateTime =
		SystemClock::GetDateTime(SystemClock::StartTime());

	std::vector<std::string> uptimeParts;
	if (dateTime.day > 0) {
		uptimeParts.emplace_back(std::to_string(dateTime.day) + " days,");
	}
	if (dateTime.hour > 0) {
		uptimeParts.emplace_back(std::to_string(dateTime.hour) + " hours,");
	}
	if (dateTime.minute > 0) {
		uptimeParts.emplace_back(std::to_string(dateTime.minute) + " mins,");
	}
	if (dateTime.second > 0) {
		uptimeParts.emplace_back(std::to_string(dateTime.second) + " sec");
	}
	const std::string uptime = "Uptime:  " + Unnamed::StrUtil::Join(
		                           uptimeParts, " "
	                           );

	const std::vector<std::string> info = {
		prompt + "\n",
		(!prompt.empty() ? std::string(prompt.size(), '-') : "") + "\n",
		uptime + "\n",
		"Resolution:  " +
		std::to_string(0) +
		"x" +
		std::to_string(0) +
		"\n",
		"CPU:  " + WindowsUtils::GetCPUName() + "\n",
		"GPU:  " + WindowsUtils::GetGPUName() + "\n",
		"Memory:  " + WindowsUtils::GetRamUsage() + " / " +
		WindowsUtils::GetRamMax() + "\n",
		"\n",
		"\n",
		"\n",
		"\n",
		"\n",
		"\n",
		"\n",
		"\n",
		"\n",
	};

	// 結合結果を格納するベクトル
	std::vector<std::string> combined;

	// アスキーアートとシステム情報の行数の最大値を取得
	const size_t maxRows    = std::max(asciiArt.size(), info.size());
	const size_t asciiWidth = asciiArt[0].size();

	// 結合処理
	for (size_t i = 0; i < maxRows; ++i) {
		constexpr size_t padding  = 5;
		std::string      leftPart = (i < asciiArt.size()) ?
			                            asciiArt[i] :
			                            std::string(asciiWidth, ' ');
		std::string rightPart = (i < info.size()) ? info[i] : "";
		combined.emplace_back(leftPart + std::string(padding, ' ') + rightPart);
	}

	// 結合結果を一行ずつ出力
	for (const std::string& line : combined) {
		Print(line, kConTextColorWait, Channel::None);
	}
}

/// @brief 引数をそのままコンソールに出力します
/// @param args コマンド引数
void Console::Echo(const std::vector<std::string>& args) {
	Print(
		Unnamed::StrUtil::Join(args, " ") + "\n", kConFgColorDark,
		Channel::Console
	);
}

/// @brief コンソールのバッファを取得します
/// @return コンソールのバッファ
std::vector<std::string> Console::GetBuffer() {
#ifdef _DEBUG
	return mMessageBuffer;
#else
	return {};
#endif
}

#ifdef _DEBUG
/// @brief 入力に基づいてサジェストを更新します
/// @param input 現在の入力文字列
void Console::UpdateSuggestions(const std::string& input) {
	mSuggestions.clear();

	// 入力が空の場合はサジェストしない
	if (input.empty()) {
		for (const auto& command : ConCommand::GetCommands()) {
			mSuggestions.emplace_back(command.first);
		}
		for (const auto& conVar : ConVarManager::GetAllConVars()) {
			mSuggestions.emplace_back(conVar->GetName());
		}
		return;
	}

	for (const auto& command : ConCommand::GetCommands()) {
		if (command.first.starts_with(input)) {
			mSuggestions.emplace_back(command.first);
		}
	}

	for (const auto& conVar : ConVarManager::GetAllConVars()) {
		if (conVar->GetName().starts_with(input)) {
			mSuggestions.emplace_back(conVar->GetName());
		}
	}
}
#endif

#ifdef _DEBUG
/// @brief サジェストポップアップを表示します
void Console::ShowSuggestPopup() {
	if (mSuggestions.empty()) { return; }

	// ポップアップの位置とサイズを計算
	ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
	ImVec2 inputTextSize   = ImGui::GetItemRectSize();
	auto   popupPos        = ImVec2(
		cursorScreenPos.x,
		cursorScreenPos.y + inputTextSize.y
	);
	auto popupSize = ImVec2(
		inputTextSize.x,
		ImGui::GetTextLineHeight() * (std::min)(
			mSuggestions.size(),
			static_cast<size_t>(10)
		) +
		ImGui::GetStyle().WindowPadding.y * 2
	);

	// ポップアップウィンドウの表示
	ImGui::SetNextWindowPos(popupPos);
	ImGui::SetNextWindowSize(popupSize);

	if (ImGui::Begin(
		"Suggestions", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_Tooltip
	)) {
		for (size_t i = 0; i < mSuggestions.size(); i++) {
			if (ImGui::Selectable(mSuggestions[i].c_str())) {
				// 選択されたサジェストで入力を置き換え
				strncpy_s(
					mInputText, mSuggestions[i].c_str(),
					sizeof(mInputText)
				);
				mShowSuggestPopup = false;
			}
		}
	}
	ImGui::End();
}
#endif

#ifdef _DEBUG
/// @brief サジェストポップアップを表示します
/// @param state サジェストポップアップの状態
/// @param pos ポップアップの位置
/// @param size ポップアップのサイズ
/// @param isFocused ポップアップがフォーカスされているかどうか
void Console::SuggestPopup(
	[[maybe_unused]] SuggestPopupState& state, const ImVec2&         pos,
	const ImVec2&                       size, [[maybe_unused]] bool& isFocused
) {
	// 角丸をなくす
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar |          // タイトルバーなし
		ImGuiWindowFlags_NoResize |            // リサイズしない
		ImGuiWindowFlags_NoMove |              // 移動しない
		ImGuiWindowFlags_HorizontalScrollbar | // 水平スクロールバー
		ImGuiWindowFlags_NoSavedSettings;      // iniファイルに書き込まない

	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowSize(size);
	ImGui::Begin("suggestPopup", nullptr, flags);
	ImGui::SetKeyboardFocusHere();

	for (size_t i = 0; i < kConsoleSuggestLineCount; ++i) {
		size_t test = ConVarManager::GetAllConVars().size();
		if (test <= i) { break; }

		bool isIndexActive = state.activeIndex == static_cast<int>(i);
		if (isIndexActive) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 0, 0, 1));
		}

		ImGui::PushID(static_cast<int>(i));
		if (ImGui::Selectable(
			ConVarManager::GetAllConVars()[i]->GetName().c_str(),
			isIndexActive
		)) { state.clickedIndex = static_cast<int>(i); }
		ImGui::PopID();

		if (isIndexActive) { ImGui::PopStyleColor(); }
	}

	ImGui::PopStyleVar();
	ImGui::End();
}
#endif

#ifdef _DEBUG
/// @brief 入力テキストのコールバック関数
/// @param data コールバックデータ
/// @return 0を返します
int Console::InputTextCallback(ImGuiInputTextCallbackData* data) {
	switch (data->EventFlag) {
		case ImGuiInputTextFlags_CallbackCompletion: {
			// bShowSuggestPopup_ = !bShowSuggestPopup_;
			if (mShowSuggestPopup) { UpdateSuggestions(data->Buf); }
		}
		break;

		case ImGuiInputTextFlags_CallbackHistory: {
			const int prev_history_index = mHistoryIndex;
			if (data->EventKey == ImGuiKey_UpArrow) {
				if (mHistoryIndex > 0) { mHistoryIndex--; }
			} else if (data->EventKey == ImGuiKey_DownArrow) {
				if (mHistoryIndex < static_cast<int>(mHistory.size()) - 1) {
					mHistoryIndex++;
				} else {
					mHistoryIndex = static_cast<int>(mHistory.size());
					// 履歴が空の場合はサイズと一致させる
				}
			}
			if (prev_history_index != mHistoryIndex) {
				data->DeleteChars(0, data->BufTextLen);
				if (mHistoryIndex < static_cast<int>(mHistory.size())) {
					data->InsertChars(0, mHistory[mHistoryIndex].c_str());
				} else {
					data->InsertChars(0, ""); // 履歴が空の場合は空白を挿入
				}
			}
		}
		break;

		case ImGuiInputTextFlags_CallbackEdit: Print(
				"Edit\n", kConsoleColorInt
			);
			break;

		case ImGuiInputTextFlags_CallbackResize: Print(
				"Resize\n", kConTextColorError
			);
			break;
		default: ;
	}
	return 0;
}
#endif

/// @brief コンソールのメニューバーを表示します
void Console::ShowMenuBar() {
#ifdef _DEBUG
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Clear", "Ctrl+C")) { Clear({}); }
			if (ImGui::MenuItem("Close", "Ctrl+W")) { mShowConsole = false; }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("Help", "F1")) { Help({}); }
			if (ImGui::MenuItem(
				(Unnamed::StrUtil::ConvertToUtf8(0xeb8e) + " About Console").
				c_str()
			)) { mShowAbout = true; }
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
#endif
}

/// @brief コンソールのテキスト部分を表示します
void Console::ShowConsoleText() {
#ifdef _DEBUG
	std::lock_guard lock(mMutex);

	constexpr size_t kMaxConsoleLines = 100000;
	// まずconsoleTexts_のサイズ上限をチェックし、超えていたらクリア
	if (mConsoleTexts.size() > kMaxConsoleLines) {
		Print(
			"Console buffer overflow, clearing buffer.\n", kConTextColorError,
			Channel::Console
		);
		mConsoleTexts.clear();
	}

	// 60FPSを超えない頻度でバッファを更新
	const size_t currentFrame = mFrameCount;
	if (currentFrame - mDisplayState.lastUpdateFrame > 1) {
		mDisplayState.buffer = mConsoleTexts;
		mDisplayState.selected.resize(mDisplayState.buffer.size(), false);
		mDisplayState.lastUpdateFrame = currentFrame;
	}

	// displayState_.bufferのサイズ上限もチェック
	if (mDisplayState.buffer.size() > kMaxConsoleLines) {
		Print(
			"Console buffer overflow, clearing buffer.\n", kConTextColorError,
			Channel::Console
		);
		mDisplayState.buffer.clear();
	}
	if (mDisplayState.buffer.size() > kMaxConsoleLines) {
		Print(
			"Console buffer still too large after clear.\n",
			kConTextColorError, Channel::Console
		);
	} else {
		mDisplayState.selected.resize(mDisplayState.buffer.size(), false);
	}

	// 入力フィールドとボタンの高さを取得
	float inputTextHeight = ImGui::GetFrameHeightWithSpacing();
	// 子ウィンドウの高さを調整
	auto childSize = ImVec2(
		0, -inputTextHeight - ImGui::GetStyle().ItemSpacing.y
	);

	if (ImGui::BeginTable(
		"ConsoleTable", 2,
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY |
		ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit,
		childSize
	)) {
		// ヘッダー
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Log", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		// フィルタリング後の行インデックスを追跡するための変数
		int visibleIndex = 0;

		// データ行
		for (size_t i = 0; i < mDisplayState.buffer.size(); ++i) {
			// チャンネルによるフィルター処理
			if (mCurrentFilterChannel != Channel::None && mDisplayState.buffer[
				    i].channel != mCurrentFilterChannel) {
				continue; // フィルターに一致しない場合はスキップ
			}

			ImGui::TableNextRow();

			// チャンネル列
			if (mDisplayState.buffer[i].channel != Channel::None) {
				if (ImGui::TableSetColumnIndex(0)) {
					std::string channelName = ToString(
						mDisplayState.buffer[i].channel
					);
					if (!channelName.empty()) {
						if (ImGui::TextLink(
							(channelName + "##" + std::to_string(i)).c_str()
						)) {
							// チャンネルをクリックした時のフィルター設定
							if (mCurrentFilterChannel == mDisplayState.buffer[i]
							    .channel) {
								mCurrentFilterChannel = Channel::None;
								// 同じチャンネルをクリックするとフィルター解除
							} else {
								mCurrentFilterChannel = mDisplayState.buffer[i].
									channel;
							}
						}
					}
				}
			} else { ImGui::TableSetColumnIndex(0); }

			// ログ列
			if (ImGui::TableSetColumnIndex(1)) {
				if (WindowsUtils::IsAppDarkTheme()) {
					if (mFocusedConsoleWindow) {
						ImGui::PushStyleColor(
							ImGuiCol_Text,
							ImGuiUtil::ToImVec4(
								mDisplayState.buffer[i].
								color
							)
						);
					} else {
						Vec4 color = mDisplayState.buffer[i].color;
						color.w    = 0.5f;
						ImGui::PushStyleColor(
							ImGuiCol_Text,
							ImGuiUtil::ToImVec4(color)
						);
					}
				} else {
					Vec4 color = mDisplayState.buffer[i].color;
					Vec3 col   = {color.x, color.y, color.z};
					col        *= 0.5f;
					ImGui::PushStyleColor(
						ImGuiCol_Text,
						ImGuiUtil::ToImVec4(Vec4(col, 1.0f))
					);
				}
				bool isSelected = mDisplayState.selected[i];
				if (ImGui::Selectable(
					(mDisplayState.buffer[i].text + "##" + std::to_string(i)).
					c_str(), isSelected
				)) {
					if (ImGui::GetIO().KeyCtrl) {
						// 選択状態のトグル
						mDisplayState.selected[i] = !mDisplayState.selected[i];
					} else if (ImGui::GetIO().KeyShift && mLastSelectedIndex !=
					           -1) {
						// フィルタリング後の範囲選択
						const int start = std::min(
							mLastSelectedIndex, visibleIndex
						);
						const int end = std::max(
							mLastSelectedIndex, visibleIndex
						);
						for (int j = start; j <= end; ++j) {
							size_t actualIndex = FilteredToActualIndex(j);
							if (actualIndex != SIZE_MAX) {
								mDisplayState.selected[actualIndex] = true;
							}
						}
					} else {
						// 単一選択（フィルタリング後の要素に限定）
						std::ranges::fill(mDisplayState.selected, false);
						mDisplayState.selected[i] = true;
					}
					mLastSelectedIndex = visibleIndex; // フィルタリング後のインデックス
				}
				ImGui::PopStyleColor();
			}
			visibleIndex++; // フィルタリング後のインデックスを更新
		}

		ShowContextMenu();

		// Ctrl+C でコピー
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
			std::string      copiedText;
			constexpr size_t kMaxCopyLength = 1024 * 1024; // 1MBまで
			for (size_t i = 0; i < mDisplayState.buffer.size(); ++i) {
				if (mDisplayState.selected[i]) {
					if (copiedText.size() + mDisplayState.buffer[i].text.size()
					    > kMaxCopyLength) {
						copiedText += "\n... (copy truncated) ...";
						break;
					}
					copiedText += mDisplayState.buffer[i].text;
				}
			}
			ImGui::SetClipboardText(copiedText.c_str());
		}

		CheckScroll();

		ImGui::EndTable();
	}
#endif
}

/// @brief コンソールの本文部分を表示します
void Console::ShowConsoleBody() {
#ifdef _DEBUG
	ImGui::PushStyleColor(ImGuiCol_Border, {0.0f, 0.0f, 0.0f, 0.0f});
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, {0.0f, 0.0f, 0.0f, 0.0f});

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 3.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 16.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 128.0f);

	ShowConsoleText();

	ImGui::Spacing();

	ImVec2      size        = ImGui::GetContentRegionAvail();
	const float buttonWidth = ImGui::CalcTextSize(" Submit ").x +
	                          ImGui::GetStyle().FramePadding.x * 2.0f;
	size.x -= buttonWidth + ImGui::GetStyle().ItemSpacing.x;

	ImGui::SetNextItemWidth(size.x);
	ImGuiInputTextFlags inputTextFlags =
		ImGuiInputTextFlags_EnterReturnsTrue |
		ImGuiInputTextFlags_EscapeClearsAll |
		ImGuiInputTextFlags_CallbackAlways |
		ImGuiInputTextFlags_CallbackCharFilter |
		ImGuiInputTextFlags_CallbackCompletion |
		ImGuiInputTextFlags_CallbackEdit |
		ImGuiInputTextFlags_CallbackHistory;

	if (ImGui::InputText(
		"##input", mInputText, IM_ARRAYSIZE(mInputText),
		inputTextFlags, InputTextCallback
	)) {
		ImGui::SetKeyboardFocusHere(-1);
		SubmitCommand(mInputText);
		if (!std::string(mInputText).empty()) {
			mHistory.emplace_back(mInputText);
			mHistoryIndex = static_cast<int>(mHistory.size());
		}
		memset(mInputText, 0, sizeof mInputText);
	}

	ShowSuggestPopup();

	ImGui::SameLine();

	if (ImGui::Button(" Submit ")) {
		SubmitCommand(mInputText);
		if (!std::string(mInputText).empty()) {
			mHistory.emplace_back(mInputText);
			mHistoryIndex = static_cast<int>(mHistory.size());
		}
		memset(mInputText, 0, sizeof mInputText);
	}

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
#endif
}

/// @brief コンソールのコンテキストメニューを表示します
void Console::ShowContextMenu() {
#ifdef _DEBUG
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
	ImGui::PushStyleColor(ImGuiCol_Border, {0.25f, 0.25f, 0.25f, 1.0f});

	// 選択された全テキストをコピーする機能（右クリックメニューから）
	if (ImGui::BeginPopupContextWindow()) {
		ImGui::Spacing();

		if (ImGui::MenuItem(
			(Unnamed::StrUtil::ConvertToUtf8(kIconCopy) + " Copy Selected").
			c_str()
		)) {
			std::string      copiedText;
			constexpr size_t kMaxCopyLength = 1024 * 1024; // 1MBまで
			for (size_t i = 0; i < mDisplayState.buffer.size(); ++i) {
				if (mDisplayState.selected[i]) {
					if (copiedText.size() + mDisplayState.buffer[i].text.size()
					    > kMaxCopyLength) {
						copiedText += "\n... (copy truncated) ...";
						break;
					}
					copiedText += mDisplayState.buffer[i].text;
				}
			}
			ImGui::SetClipboardText(copiedText.c_str());
		}

		ImGui::Separator();

		if (ImGui::MenuItem(
			(Unnamed::StrUtil::ConvertToUtf8(kIconVisibility) +
			 " Show All Channels").
			c_str()
		)) { mCurrentFilterChannel = Channel::None; }

		ImGui::Spacing();
		ImGui::EndPopup();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
#endif
}

/// @brief コンソールの「About」ポップアップを表示します
void Console::ShowAbout() {
#ifdef _DEBUG
	ImGui::OpenPopup(
		("About " + std::string(ENGINE_NAME) + " Console").c_str()
	);

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize({280.0f, 230.0f}, ImGuiCond_FirstUseEver);

	const bool bOpen = ImGui::BeginPopupModal(
		("About " + std::string(ENGINE_NAME) + " Console").c_str(),
		&mShowAbout,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings
	);

	if (bOpen) {
		ImGui::Text(
			(Unnamed::StrUtil::ConvertToUtf8(kIconTerminal) +
			 " Unnamed Console").
			c_str()
		);
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Version: %s", std::string(ENGINE_VERSION).c_str());
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text(
			"Build: %s %s", std::string(__DATE__).c_str(),
			std::string(__TIME__).c_str()
		);
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("ImGui Version: %s", ImGui::GetVersion());
		ImGui::Spacing();
		ImGui::Spacing();
		// ボタンサイズとウィンドウサイズを取得
		auto   buttonSize     = ImVec2(74.0f, 24.0f);
		ImVec2 windowSize     = ImGui::GetWindowSize(); // ウィンドウ全体のサイズ
		ImVec2 cursorStartPos = ImGui::GetCursorPos();  // 現在のカーソル位置

		// ボタンを中央に配置
		float buttonPosX = (windowSize.x - buttonSize.x) * 0.5f; // 中央揃えのX座標
		float buttonPosY = cursorStartPos.y + ImGui::GetContentRegionAvail().y -
		                   buttonSize.y; // 下端からの位置調整
		ImGui::SetCursorPos(ImVec2(buttonPosX, buttonPosY));

		if (ImGui::Button("OK", buttonSize)) {
			mShowAbout = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
#endif
}

/// @brief ConVarヘルパーウィンドウを表示します
void Console::ShowConVarHelper() {
#ifdef _DEBUG
	if (!mShowConVarHelper) { return; }

	ImGui::SetNextWindowSizeConstraints(
		{320.0f, 0.0f},
		{0xFFFF, 0xFFFF}
	);

	if (ImGui::Begin(
		"ConVar Helper", &mShowConVarHelper,
		ImGuiWindowFlags_MenuBar
	)) {
		// メニューバー
		{
			if (ImGui::BeginMenuBar()) {
				if (ImGui::BeginMenu("File")) {
					if (ImGui::MenuItem(
						(Unnamed::StrUtil::ConvertToUtf8(kIconDownload) +
						 " Import Page").c_str()
					)) { ImportPage(); }
					ImGui::BeginDisabled();
					if (ImGui::MenuItem(
						(Unnamed::StrUtil::ConvertToUtf8(kIconUpload) +
						 " Export Page").
						c_str()
					)) { ExportPage(); }
					ImGui::EndDisabled();
					ImGui::EndMenu();
				}
				ImGui::EndMenuBar();
			}
		}

		// ページ選択
		{
			// ComboBoxの表示
			ImGui::Text("Page");
			ImGui::SameLine();
			if (ImGui::BeginCombo(
				"##Page",
				mPages[mSelectedPageIndex].name.c_str(),
				ImGuiComboFlags_WidthFitPreview
			)) {
				for (uint32_t n = 0; n < mPages.size(); n++) {
					const bool isSelected = (static_cast<int>(
						                         mSelectedPageIndex) == n);
					if (ImGui::Selectable(mPages[n].name.c_str(), isSelected)) {
						mSelectedPageIndex = n;
					}
					if (isSelected) { ImGui::SetItemDefaultFocus(); }
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();

			if (ImGui::Button("+", {24, 0})) {
				Page newPage = {
					.name = "User Page " + std::to_string(mPages.size()),
					.grid = {3, 10},
				};
				mPages.emplace_back(newPage);
			}

			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("ページを追加します");
				ImGui::EndTooltip();
			}
		}

		// ページ名の編集
		{
			char pageNameBuffer[256];
			strncpy_s(
				pageNameBuffer, mPages[mSelectedPageIndex].name.c_str(),
				sizeof(pageNameBuffer)
			);
			if (ImGui::InputText(
				"PageName", pageNameBuffer,
				sizeof(pageNameBuffer)
			)) { mPages[mSelectedPageIndex].name = pageNameBuffer; }
		}

		// グリッドの設定
		{
			// ウィンドウの利用可能な幅を取得
			const float windowWidth = ImGui::GetContentRegionAvail().x;

			// 各要素の幅を計算
			const float labelWidth = ImGui::CalcTextSize("Grid").x;
			const float xTextWidth = ImGui::CalcTextSize("x").x;
			const float spacing    = ImGui::GetStyle().ItemSpacing.x;
			const float inputWidth = (windowWidth - labelWidth - xTextWidth -
			                          spacing * 4) * 0.5f;

			ImGui::Text("Grid");
			ImGui::SameLine();

			ImGui::SetNextItemWidth(inputWidth);
			int tmpWidth = static_cast<int>(mPages[mSelectedPageIndex].grid.
				width);
			if (ImGui::InputInt("##Width", &tmpWidth, 1)) {
				if (tmpWidth > 0 && tmpWidth != static_cast<int>(mPages[
					    mSelectedPageIndex].grid.width)) {
					RearrangeGridElements(
						tmpWidth,
						mPages[mSelectedPageIndex].grid.
						                           height
					);
					mPages[mSelectedPageIndex].grid.width = tmpWidth;
				}
			}

			ImGui::SameLine();
			ImGui::Text("x");
			ImGui::SameLine();

			ImGui::SetNextItemWidth(inputWidth);
			int tmpHeight = static_cast<int>(mPages[mSelectedPageIndex].grid.
				height);
			if (ImGui::InputInt("##Height", &tmpHeight, 1)) {
				mPages[mSelectedPageIndex].grid.height = std::max(1, tmpHeight);
			}
		}

		// グリッドサイズのテーブルを作成
		{
			const auto& grid = mPages[mSelectedPageIndex].grid;

			// グリッド要素の数を正しく保つ
			size_t expectedSize = static_cast<size_t>(grid.width) * grid.height;
			auto&  elements     = mPages[mSelectedPageIndex].elements;
			if (elements.size() != expectedSize) {
				elements.resize(expectedSize);
			}

			if (ImGui::BeginTable(
				"GridTable", grid.width,
				ImGuiTableFlags_Borders |
				ImGuiTableFlags_SizingFixedFit |
				ImGuiTableFlags_NoPadInnerX |
				ImGuiTableFlags_NoPadOuterX
			)) {
				// 列幅を均等に設定
				float columnWidth = ImGui::GetContentRegionAvail().x /
				                    static_cast<float>(grid.width);
				for (uint32_t i = 0; i < grid.width; ++i) {
					ImGui::TableSetupColumn(
						"", ImGuiTableColumnFlags_WidthFixed, columnWidth
					);
				}

				// スタイルのパディングとスペーシングをなくす
				ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

				// 固定のセル高さを設定

				for (uint32_t row = 0; row < grid.height; ++row) {
					constexpr float cellHeight = 20.0f;
					ImGui::TableNextRow(0, cellHeight);
					for (uint32_t col = 0; col < grid.width; ++col) {
						ImGui::TableSetColumnIndex(col);

						// セルのインデックスを計算
						size_t       cellIndex = row * grid.width + col;
						GridElement& element   = elements[cellIndex];

						// セルの内容を表示
						std::string cellId = std::format(
							"Cell ({}, {})", col, row
						);

						// ボタンのサイズをセルの幅と高さに合わせる
						auto cellSize = ImVec2(columnWidth, cellHeight);

						if (element.type == GridElement::Type::None) {
							// 要素がない場合は見えないボタンを配置してコンテキストメニューを有効にする
							ImGui::InvisibleButton(cellId.c_str(), cellSize);

							// 右クリックでコンテキストメニューを表示
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								if (ImGui::MenuItem("Add Label")) {
									mShowElementPopup = true;
									mEditingElementIndex = cellIndex;
									element.type = GridElement::Type::Label;
								}
								if (ImGui::MenuItem("Add Button")) {
									mShowElementPopup = true;
									mEditingElementIndex = cellIndex;
									element.type = GridElement::Type::Button;
								}
								ImGui::EndPopup();
							}
						} else if (element.type == GridElement::Type::Label) {
							// ラベルの場合は、テキストを表示
							ImGui::PushStyleColor(
								ImGuiCol_Button,
								ImGuiUtil::ToImVec4(element.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonActive,
								ImGuiUtil::ToImVec4(element.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonHovered,
								ImGuiUtil::ToImVec4(element.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_Text,
								ImGuiUtil::ToImVec4(element.fgColor)
							);

							//// テキストを中央揃えで配置
							//ImVec2 textSize = ImGui::CalcTextSize(element.label.c_str());
							//ImVec2 pos = ImGui::GetCursorPos();
							//pos.x += (cellSize.x - textSize.x) * 0.5f;
							//pos.y += (cellSize.y - textSize.y) * 0.5f;
							//ImGui::SetCursorPos(pos);

							ImGui::Button(
								(element.label + "##" + std::to_string(row) +
								 std::to_string(col)).c_str(), cellSize
							);
							ImGui::PopStyleColor(4);

							// 右クリックでコンテキストメニューを表示
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								if (ImGui::MenuItem("Edit Label")) {
									mShowElementPopup    = true;
									mEditingElementIndex = cellIndex;
								}
								if (ImGui::MenuItem("Delete Label")) {
									element = GridElement(); // 要素を削除
								}
								ImGui::EndPopup();
							}
						} else if (element.type == GridElement::Type::Button) {
							// ボタンの場合は、ボタンを表示
							ImGui::PushStyleColor(
								ImGuiCol_Button,
								ImGuiUtil::ToImVec4(element.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonActive,
								ImGuiUtil::ToImVec4(element.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonHovered, {
									0.33f, 0.37f, 0.44f, 1.0f
								}
							);
							ImGui::PushStyleColor(
								ImGuiCol_Text,
								ImGuiUtil::ToImVec4(element.fgColor)
							);

							if (ImGui::Button(
								(element.label + "##" + std::to_string(row) +
								 std::to_string(col)).c_str(), cellSize
							)) {
								// ボタンのクリック処理
								SubmitCommand(element.command);
							}

							if (ImGui::IsItemHovered()) {
								ImGui::SetTooltip(
									"%s", element.command.c_str()
								);
							}

							ImGui::PopStyleColor(4);

							// 右クリックでコンテキストメニューを表示
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								if (ImGui::MenuItem("Edit Button")) {
									mShowElementPopup    = true;
									mEditingElementIndex = cellIndex;
								}
								if (element.type == GridElement::Type::Label) {
									if (ImGui::MenuItem("Delete Button")) {
										element = GridElement(); // 要素を削除
									}
								}

								ImGui::Separator();

								if (ImGui::MenuItem("Copy Command")) {
									ImGui::SetClipboardText(
										element.command.c_str()
									);
								}

								ImGui::Separator();

								if (ImGui::MenuItem("Insert Row")) {
									InsertRow(row);
								}
								if (ImGui::MenuItem("Delete Row")) {
									DeleteRow(row);
								}
								ImGui::EndPopup();
							}
						}
					}
				}

				ImGui::PopStyleVar(2);
				ImGui::EndTable();
			}
		}

		// 要素編集用のポップアップを表示
		ShowElementEditPopup();
	}

	ImGui::End();
#endif
}

/// @brief グリッド要素を新しいサイズに再配置します
void Console::RearrangeGridElements(
	[[maybe_unused]] const uint32_t newWidth,
	[[maybe_unused]] const uint32_t newHeight
) {
#ifdef _DEBUG
	auto&                    elements = mPages[mSelectedPageIndex].elements;
	std::vector<GridElement> newElements(newWidth * newHeight);

	for (uint32_t row = 0; row < mPages[mSelectedPageIndex].grid.height; ++
	     row) {
		for (uint32_t col = 0; col < mPages[mSelectedPageIndex].grid.width; ++
		     col) {
			uint32_t oldIndex = row * mPages[mSelectedPageIndex].grid.width +
			                    col;
			uint32_t newIndex = row * newWidth + col;

			if (row < newHeight && col < newWidth && oldIndex < elements.size()
			    && newIndex < newElements.size()) {
				newElements[newIndex] = elements[oldIndex];
			}
		}
	}

	elements = std::move(newElements);
#endif
}

/// @brief 指定した行に新しい行を挿入します
/// @param row 挿入する行のインデックス
void Console::InsertRow([[maybe_unused]] const uint32_t row) {
#ifdef _DEBUG
	auto& grid     = mPages[mSelectedPageIndex].grid;
	auto& elements = mPages[mSelectedPageIndex].elements;

	// 新しい行を挿入
	elements.insert(
		elements.begin() + row * grid.width, grid.width,
		GridElement()
	);

	// グリッドの高さを更新
	grid.height++;
#endif
}

/// @brief 指定した行を削除します
/// @param row 削除する行のインデックス
void Console::DeleteRow([[maybe_unused]] const uint32_t row) {
#ifdef _DEBUG
	auto& grid     = mPages[mSelectedPageIndex].grid;
	auto& elements = mPages[mSelectedPageIndex].elements;

	// 行を削除
	elements.erase(
		elements.begin() + row * grid.width,
		elements.begin() + (row + 1) * grid.width
	);

	// グリッドの高さを更新
	grid.height--;
#endif
}

/// @brief ページをINIファイルからインポートします
void Console::ImportPage() {
#ifdef _DEBUG
	// カレントディレクトリを保存
	WCHAR currentDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, currentDir);

	OPENFILENAME ofn;
	WCHAR        szFile[260] = {};
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize     = sizeof(ofn);
	ofn.hwndOwner       = nullptr;
	ofn.lpstrFile       = szFile;
	ofn.nMaxFile        = sizeof(szFile);
	ofn.lpstrFilter     = L"Files (*.ini)\0*.ini\0";
	ofn.nFilterIndex    = 1;
	ofn.lpstrFileTitle  = nullptr;
	ofn.nMaxFileTitle   = 0;
	ofn.lpstrInitialDir = nullptr;
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
	ofn.lpstrTitle      = L"Open File";

	if (GetOpenFileName(&ofn) == TRUE) {
		std::ifstream file(ofn.lpstrFile);
		if (file.is_open()) {
			Unnamed::IniParser parser;
			auto               iniData = parser.ParseIniFile(
				Unnamed::StrUtil::ToString(ofn.lpstrFile)
			);

			// Generalセクションからグリッドサイズを取得
			auto general = iniData.find("General");
			if (general != iniData.end()) {
				// GridWidth
				auto gridWidth = general->second.find("GridWidth");
				if (gridWidth != general->second.end()) {
					mPages[mSelectedPageIndex].grid.width = std::stoi(
						gridWidth->second
					);
				}
				// GridHeight
				auto gridHeight = general->second.find("GridHeight");
				if (gridHeight != general->second.end()) {
					mPages[mSelectedPageIndex].grid.height = std::stoi(
						gridHeight->second
					);
				}
			}

			// 要素を初期化
			mPages[mSelectedPageIndex].elements.clear();
			size_t totalElements = mPages[mSelectedPageIndex].grid.width *
			                       mPages[mSelectedPageIndex].grid.height;
			mPages[mSelectedPageIndex].elements.resize(totalElements);

			// 各セクションを処理
			for (const auto& [key, value] : iniData.at("General")) {
				std::vector<std::string> tokens = TokenizeKey(key);
				if (tokens.size() < 4) { continue; }

				uint32_t           col      = std::stoul(tokens[1]);
				uint32_t           row      = std::stoul(tokens[2]);
				const std::string& property = tokens[3];

				// インデックス計算
				uint32_t index = row * mPages[mSelectedPageIndex].grid.width +
				                 col;
				if (index >= mPages[mSelectedPageIndex].elements.size()) {
					Print(
						"インデックスが範囲外です: " + key + "\n", kConTextColorError,
						Channel::Console
					);
					continue;
				}

				GridElement& element = mPages[mSelectedPageIndex].elements[
					index];

				// プロパティに基づいて値を設定
				if (property == "Label") {
					element.type    = GridElement::Type::Label;
					element.bgColor = kConHelperColorLabelBg;
					element.label   = value;
				} else if (property == "AltButtonText") {
					element.type  = GridElement::Type::Button;
					element.label = value;
				} else if (
					property == "Command") { element.command = value; } else if
				(property ==
				 "Description") { element.description = value; } else if (
					property == "BGColor") {
					element.bgColor = ParseColor(value);
				} else if (property == "FGColor") {
					element.fgColor = ParseColor(value);
				} else {
					//Print("不明なプロパティ: " + key + "\n", kConTextColorError, Channel::Console);
				}

				mPages[mSelectedPageIndex].elements[index] = element;
			}

			file.close();
		} else {
			Print("ファイルが開けませんでした。\n", kConTextColorError, Channel::Console);
		}
	} else {
		Print("ファイルが選択がキャンセルされました。\n", kConTextColorError, Channel::Console);
	}

	SetCurrentDirectory(currentDir);
#endif
}

/// @brief ページをINIファイルにエクスポートします
void Console::ExportPage() {
#ifdef _DEBUG
	// カレントディレクトリを保存
	WCHAR currentDir[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, currentDir);

	OPENFILENAME ofn;
	WCHAR        szFile[260] = {};
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize     = sizeof(ofn);
	ofn.hwndOwner       = nullptr;
	ofn.lpstrFile       = szFile;
	ofn.nMaxFile        = sizeof(szFile);
	ofn.lpstrFilter     = L"INI Files (*.ini)\0*.ini\0All Files (*.*)\0*.*\0";
	ofn.nFilterIndex    = 1;
	ofn.lpstrFileTitle  = nullptr;
	ofn.nMaxFileTitle   = 0;
	ofn.lpstrInitialDir = nullptr;
	ofn.Flags           = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt     = L"ini";
	ofn.lpstrTitle      = L"Save ConVar Helper Page";

	if (GetSaveFileName(&ofn) == TRUE) {
		std::ofstream file(ofn.lpstrFile);
		if (file.is_open()) {
			// 現在のページを取得
			const auto& page = mPages[mSelectedPageIndex];

			// ヘッダーを書き込む
			file << "[General]" << std::endl;
			file << "GridWidth=" << page.grid.width << std::endl;
			file << "GridHeight=" << page.grid.height << std::endl;
			file << "PageName=" << page.name << std::endl;
			file << std::endl;

			// 各要素の設定を書き込む
			for (uint32_t row = 0; row < page.grid.height; ++row) {
				for (uint32_t col = 0; col < page.grid.width; ++col) {
					size_t index = row * page.grid.width + col;
					if (index >= page.elements.size()) { continue; }

					const GridElement& element = page.elements[index];

					// 要素が設定されていない場合はスキップ
					if (element.type == GridElement::Type::None) { continue; }

					// タイプに応じた情報を書き込む
					file << "Element-" << col << "-" << row << "-Type=";
					if (element.type == GridElement::Type::Label) {
						file << "Label" << std::endl;
					} else if (element.type == GridElement::Type::Button) {
						file << "Button" << std::endl;
					}

					file << "Element-" << col << "-" << row << "-Label=" <<
						element.label << std::endl;

					if (element.type == GridElement::Type::Button) {
						file << "Element-" << col << "-" << row << "-Command="
							<< element.command << std::endl;
					}

					if (!element.description.empty()) {
						file << "Element-" << col << "-" << row <<
							"-Description=" << element.description << std::endl;
					}

					// 色情報のエンコード
					file << "Element-" << col << "-" << row << "-BGColor=" <<
						FormatColorForIni(element.bgColor) << std::endl;
					file << "Element-" << col << "-" << row << "-FGColor=" <<
						FormatColorForIni(element.fgColor) << std::endl;

					file << std::endl;
				}
			}

			file.close();
			Print(
				"ページを正常にエクスポートしました: " + Unnamed::StrUtil::ToString(
					ofn.lpstrFile
				) +
				"\n", kConFgColorDark, Channel::Console
			);
		} else {
			Print("ファイルを作成できませんでした。\n", kConTextColorError, Channel::Console);
		}
	} else {
		// ユーザーがキャンセルした場合やエラーが発生した場合
		if (CommDlgExtendedError() != 0) {
			Print(
				"ファイル保存ダイアログでエラーが発生しました。\n", kConTextColorError,
				Channel::Console
			);
		}
	}

	// カレントディレクトリを元に戻す
	SetCurrentDirectory(currentDir);
#endif
}

///	@brief Vec4の色情報をINIファイル用の文字列にフォーマットします
/// @param color フォーマットする色
/// @return フォーマットされた文字列
std::string Console::FormatColorForIni(const Vec4& color) {
	// Vec4の各要素をバイト配列に変換
	uint8_t bytes[16];

	// 各float値をバイト配列に変換
	for (int i = 0; i < 4; ++i) {
		float component = i == 0 ?
			                  color.x :
			                  (i == 1 ? color.y : (i == 2 ? color.z : color.w));
		uint32_t val = *reinterpret_cast<const uint32_t*>(&component);

		bytes[i * 4]     = static_cast<uint8_t>(val & 0xFF);
		bytes[i * 4 + 1] = static_cast<uint8_t>((val >> 8) & 0xFF);
		bytes[i * 4 + 2] = static_cast<uint8_t>((val >> 16) & 0xFF);
		bytes[i * 4 + 3] = static_cast<uint8_t>((val >> 24) & 0xFF);
	}

	// バイト配列をエスケープシーケンスを使って文字列に変換
	std::stringstream ss;
	ss << "(";
	for (int i = 0; i < 16; ++i) {
		ss << "\\x" << std::hex << std::setw(2) << std::setfill('0') <<
			static_cast<int>(bytes[i]);
	}
	ss << ")";

	return ss.str();
}

/// @brief キー文字列をトークンに分割します
/// @param key 分割するキー文字列
/// @return トークンのベクター
std::vector<std::string> Console::TokenizeKey(const std::string& key) {
	std::vector<std::string> tokens;
	std::stringstream        ss(key);
	std::string              token;

	while (std::getline(ss, token, '-')) { tokens.emplace_back(token); }

	return tokens;
}

/// @brief INIファイルから読み込んだ色情報の文字列をVec4に変換します
/// @param color 変換する色情報の文字列
/// @return 変換されたVec4の色
Vec4 Console::ParseColor(const std::string& color) {
	// 1. 括弧内の内容を取得
	size_t start = color.find('(');
	size_t end   = color.rfind(')');
	if (start == std::string::npos || end == std::string::npos || end <=
	    start) {
		Print(
			"ParseColor: 無効な形式の文字列です: " + color + "\n", kConTextColorError,
			Channel::Console
		);
		return Vec4(0.0f, 0.0f, 0.0f, 1.0f); // デフォルトの色を返す
	}
	std::string content = color.substr(start + 1, end - start - 1);

	// 2. エスケープシーケンスを展開してバイト配列を取得
	std::vector<uint8_t> bytes;
	for (size_t i = 0; i < content.size();) {
		if (content[i] == '\\') {
			i++;
			if (i >= content.size()) break;
			char c = content[i];
			if (c == 'x') {
				// 16進数エスケープ (\xHH)
				if (i + 2 < content.size()) {
					std::string hexStr = content.substr(i + 1, 2);
					uint8_t     val    = static_cast<uint8_t>(std::stoul(
						hexStr, nullptr, 16
					));
					bytes.emplace_back(val);
					i += 3; // '\\', 'x', 'HH' の分を消費
				} else {
					break; // 不完全なエスケープシーケンス
				}
			} else if (std::isdigit(static_cast<unsigned char>(c))) {
				// 8進数エスケープ (\NNN)
				std::string octStr;
				int         digits = 0;
				while (i < content.size() && digits < 3 && std::isdigit(
					       static_cast<unsigned char>(content[i])
				       )) {
					octStr += content[i];
					i++;
					digits++;
				}
				uint8_t val = static_cast<uint8_t>(std::stoul(
					octStr, nullptr, 8
				));
				bytes.emplace_back(val);
			} else {
				// その他のエスケープシーケンス
				switch (c) {
					case 'n': bytes.emplace_back('\n');
						i++;
						break;
					case 't': bytes.emplace_back('\t');
						i++;
						break;
					case '\\': bytes.emplace_back('\\');
						i++;
						break;
					default: bytes.emplace_back(static_cast<uint8_t>(c));
						i++;
						break;
				}
			}
		} else {
			bytes.emplace_back(static_cast<uint8_t>(content[i]));
			i++;
		}
	}

	// 3. バイト配列から色情報を取得
	if (bytes.size() < 16) {
		Print(
			"ParseColor: データサイズが不足しています: " + color + "\n", kConTextColorError,
			Channel::Console
		);
		return Vec4(0.0f, 0.0f, 0.0f, 1.0f); // デフォルトの色を返す
	}

	// バイト配列から float 値を取得（リトルエンディアンを想定）
	float components[4];
	for (int i = 0; i < 4; ++i) {
		uint32_t val = static_cast<uint32_t>(bytes[i * 4]) |
		               (static_cast<uint32_t>(bytes[i * 4 + 1]) << 8) |
		               (static_cast<uint32_t>(bytes[i * 4 + 2]) << 16) |
		               (static_cast<uint32_t>(bytes[i * 4 + 3]) << 24);
		components[i] = *reinterpret_cast<float*>(&val);
	}

	return Vec4(components[0], components[1], components[2], components[3]);
}

///	@brief グリッド要素の編集ポップアップを表示します
void Console::ShowElementEditPopup() {
#ifdef _DEBUG
	if (!mShowElementPopup) { return; }

	// フルスクリーンポップアップの設定
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize |
	                         ImGuiWindowFlags_NoMove;

	// ビューポート中心に表示
	auto   size   = ImVec2(400, 200);
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(size);

	auto& element = mPages[mSelectedPageIndex].elements[mEditingElementIndex];

	if (element.type == GridElement::Type::Label) {
		ImGui::OpenPopup("Edit Label");

		// ポップアップ名を "Edit Element" に統一
		if (ImGui::BeginPopupModal("Edit Label", &mShowElementPopup, flags)) {
			char labelBuffer[256];
			strncpy_s(labelBuffer, element.label.c_str(), sizeof(labelBuffer));
			if (ImGui::InputText("Label", labelBuffer, sizeof(labelBuffer))) {
				element.label = labelBuffer;
			}

			ImGui::ColorEdit4("Foreground ", &element.fgColor.x);
			ImGui::ColorEdit4("Background ", &element.bgColor.x);

			// OK と Cancel ボタン
			if (ImGui::Button("OK")) {
				mShowElementPopup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				mShowElementPopup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	} else if (element.type == GridElement::Type::Button) {
		ImGui::OpenPopup("Edit ConVar");

		if (ImGui::BeginPopupModal("Edit ConVar", &mShowElementPopup, flags)) {
			char commandBuffer[256];
			strncpy_s(
				commandBuffer, element.command.c_str(),
				sizeof(commandBuffer)
			);
			if (ImGui::InputText(
				"ConVar/ConCommand", commandBuffer,
				sizeof(commandBuffer)
			)) { element.command = commandBuffer; }

			char labelBuffer[256];
			strncpy_s(labelBuffer, element.label.c_str(), sizeof(labelBuffer));
			if (ImGui::InputText("Label", labelBuffer, sizeof(labelBuffer))) {
				element.label = labelBuffer;
			}

			ImGui::ColorEdit4("Foreground ", &element.fgColor.x);
			ImGui::ColorEdit4("Background ", &element.bgColor.x);

			// OK と Cancel ボタン
			if (ImGui::Button("OK")) {
				mShowElementPopup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				mShowElementPopup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
#endif
}

/// @brief InputSystemにコマンドを送信します
/// @param command 送信するコマンド
/// @return コマンドが有効か?
bool Console::ProcessInputCommand(const std::string& command) {
	if (command.empty() || (command[0] != '+' && command[0] != '-')) {
		return false;
	}

	InputSystem::ExecuteCommand(command, command[0] == '+');
	return true;
}

/// @brief 指定された値が指定された型に変換可能かを検証します
/// @param value 検証する値の文字列
/// @param type 期待される型 ("int", "float", "bool")
/// @return 変換可能であれば true、そうでなければ false
bool Console::ValidateType(const std::string& value, const std::string& type) {
	try {
		if (type == "int") {
			// nodiscard対策
			[[maybe_unused]] int val = std::stoi(value);
		} else if (type == "float") {
			[[maybe_unused]] float val = std::stof(value);
		} else if (type == "bool") {
			return value == "true" || value == "false";
		}
		return true;
	} catch (...) { return false; }
}

/// @brief 型変換エラーをコンソールに表示します
/// @param type エラーが発生した型
void Console::PrintTypeError(const std::string& type) {
	Print("CVAR型変換エラー: 指定された型が無効です。\n", kConTextColorError, Channel::Console);
	Print("期待される型: " + type + "\n", GetConVarTypeColor(type), Channel::Console);
}

/// @brief コマンド履歴にコマンドを追加します
/// @param command 追加するコマンド文字列
void Console::AddCommandHistory([[maybe_unused]] const std::string& command) {
#ifdef _DEBUG
	mConsoleTexts.emplace_back(
		Text(
			"> " + command + "\n",
			kConTextColorExecute,
			Channel::Console
		)
	);
#endif
}

/// @brief リピートカウントを更新します
/// @param message メッセージ文字列
/// @param hasNewLine 改行が含まれているか
/// @param channel チャンネル
/// @param color テキストの色
void Console::UpdateRepeatCount(
	[[maybe_unused]] const std::string& message,
	[[maybe_unused]] const bool         hasNewLine,
	[[maybe_unused]] const Channel&     channel,
	[[maybe_unused]] const Vec4&        color
) {
#ifdef _DEBUG
	mRepeatCounts.back()++;

	const auto repeatCount = mRepeatCounts.back();
	// 改行を含めない形式でメッセージを作成し、必要な場合のみ最後に追加
	auto formattedMessage = std::format(
		"{} [x{}]{}", message, repeatCount,
		hasNewLine ? "\n" : ""
	);

	if (repeatCount >= static_cast<int>(kConsoleRepeatError)) {
		mConsoleTexts.back() = {
			.text    = formattedMessage, .color = kConTextColorError,
			.channel = channel
		};
	} else if (repeatCount >= static_cast<int>(kConsoleRepeatWarning)) {
		mConsoleTexts.back() = {
			.text    = formattedMessage, .color = kConTextColorWarning,
			.channel = channel
		};
	} else {
		mConsoleTexts.back() = {
			.text = formattedMessage, .color = color, .channel = channel
		};
	}
#endif
}

/// @brief 最下部までスクロールされている場合は自動スクロールします
void Console::CheckScroll() {
#ifdef _DEBUG
	if (mWishScrollToBottom && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
		ImGui::SetScrollHereY(1.0f);
	}

	if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
		mWishScrollToBottom = false;
	} else { mWishScrollToBottom = true; }
#endif
}

/// @brief 行数をチェックし、必要に応じて削除やキャパシティ調整を行います
void Console::CheckLineCountAsync() {
	std::lock_guard lock(mMutex);
#ifdef _DEBUG
	// コンソールテキスト用の初期キャパシティを設定
	constexpr size_t kInitialCapacity            = 1024;
	constexpr size_t kGrowthFactor               = 512; // 増加量
	constexpr size_t kHistoryCapacity            = 128; // コマンド履歴の最大数
	constexpr size_t kSuggestionsInitialCapacity = 32;

	// コンソールテキストのキャパシティ管理
	if (mConsoleTexts.capacity() < kInitialCapacity) {
		mConsoleTexts.reserve(kInitialCapacity);
		mRepeatCounts.reserve(kInitialCapacity);
	}

	// 履歴のキャパシティ管理
	if (mHistory.capacity() < kHistoryCapacity) {
		mHistory.reserve(kHistoryCapacity);
	}

	// サジェストのキャパシティ管理
	if (mSuggestions.capacity() < kSuggestionsInitialCapacity) {
		mSuggestions.reserve(kSuggestionsInitialCapacity);
	}

	// 最大行数を超えた場合は前方から削除
	while (mConsoleTexts.size() > kConsoleMaxLineCount) {
		mConsoleTexts.erase(mConsoleTexts.begin());
		mRepeatCounts.erase(mRepeatCounts.begin());
	}

	// 履歴が最大数を超えた場合、古いものを削除
	if (mHistory.size() >= kHistoryCapacity) {
		mHistory.erase(mHistory.begin());
		mHistoryIndex = static_cast<int>(mHistory.size()) - 1;
	}

	// コンソールテキストのキャパシティが大きすぎる場合は再確保
	if (mConsoleTexts.capacity() > mConsoleTexts.size() + kGrowthFactor * 2) {
		std::vector newTexts(mConsoleTexts.begin(), mConsoleTexts.end());
		std::vector newCounts(mRepeatCounts.begin(), mRepeatCounts.end());
		mConsoleTexts = std::move(newTexts);
		mRepeatCounts = std::move(newCounts);
	}
#endif
}

/// @brief ConVarの型に対応する色を取得します
/// @param type 取得する型 ("bool", "int", "float", "Vec3", "string")
/// @return 対応する色のVec4
Vec4 Console::GetConVarTypeColor(const std::string& type) {
	if (type == "bool") { return kConsoleColorBool; }
	if (type == "int") { return kConsoleColorInt; }
	if (type == "float") { return kConsoleColorFloat; }
	if (type == "Vec3") { return kConsoleColorVec3; }
	if (type == "string") { return kConsoleColorString; }
	return kConFgColorDark;
}

/// @brief 文字列の前後の空白をトリムします
/// @param string トリムする文字列
/// @return トリムされた文字列
std::string Console::TrimSpaces(const std::string& string) {
	const size_t start = string.find_first_not_of(" \t\n\r");
	const size_t end   = string.find_last_not_of(" \t\n\r");

	if (start == std::string::npos || end == std::string::npos) { return ""; }

	return string.substr(start, end - start + 1);
}

/// @brief コマンド文字列をトークンに分割します
/// @param command 分割するコマンド文字列
/// @return トークンのベクター
std::vector<std::string> Console::TokenizeCommand(const std::string& command) {
	std::istringstream       stream(command);
	std::vector<std::string> tokens;
	std::string              token;

	while (stream >> token) { tokens.emplace_back(token); }

	return tokens;
}

/// @brief セミコロンで区切られた複数のコマンドを分割します
/// @param command 分割するコマンド文字列
/// @return 分割されたコマンドのベクター
std::vector<std::string> Console::SplitCommands(const std::string& command) {
	std::vector<std::string> result;
	std::string              current;
	bool                     inQuotes = false;
	for (char ch : command) {
		if (ch == '"') {
			inQuotes = !inQuotes;
			current  += ch;
		} else if (ch == ';' && !inQuotes) {
			result.emplace_back(current);
			current.clear();
		} else { current += ch; }
	}

	if (!current.empty()) { result.emplace_back(current); }

	return result;
}

/// @brief フィルタリングされたインデックスを実際のインデックスに変換します
/// @param filteredIndex フィルタリングされたインデックス
/// @return 実際のインデックス、該当なしの場合は SIZE_MAX
size_t Console::FilteredToActualIndex(
	[[maybe_unused]] const int filteredIndex
) {
#ifdef _DEBUG
	int visibleIndex = 0;
	for (size_t i = 0; i < mDisplayState.buffer.size(); ++i) {
		if (mCurrentFilterChannel == Channel::None || mDisplayState.buffer[i].
		    channel == mCurrentFilterChannel) {
			if (visibleIndex == filteredIndex) { return i; }
			visibleIndex++;
		}
	}

#endif

	return SIZE_MAX; // 該当なし
}

/// @brief ログバッファをファイルにフラッシュします
/// @param buffer フラッシュするメッセージのベクター
void Console::FlushLogBuffer(
	[[maybe_unused]] const std::vector<std::string>& buffer
) {
#ifdef _DEBUG
	if (!mLogFile.is_open()) {
		mLogFile.open("console.log", std::ios::app | std::ios::binary);
	}

	if (mLogFile.is_open()) {
		for (const auto& msg : buffer) { mLogFile << msg; }
		mLogFile.flush();
	}
#endif
}

/// @brief コンソールの非同期更新を行います
void Console::ConsoleUpdateAsync() const {
	try {
		while (!mStopThread) {
			std::vector<std::function<void()>> currentTasks;

			{
				std::unique_lock lock(mMutex);
				mCv.wait(
					lock, [this] { return !mTaskQueue.empty() || mStopThread; }
				);

				if (mStopThread) { break; }

				while (!mTaskQueue.empty()) {
					currentTasks.emplace_back(std::move(mTaskQueue.front()));
					mTaskQueue.pop();
				}
			}

			for (auto& task : currentTasks) {
				try { task(); } catch (const std::length_error& e) {
					Print(
						std::string("Length error in task: ") + e.what() +
						"\n", kConTextColorError, Channel::Console
					);
				} catch (const std::exception& e) {
					Print(
						std::string("Task exception: ") + e.what() + "\n",
						kConTextColorError, Channel::Console
					);
				} catch (...) {
					Print(
						"Task exception: Unknown error\n", kConTextColorError,
						Channel::Console
					);
				}
			}

			CheckLineCountAsync();
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
	} catch (const std::exception& e) {
		Print(
			std::string("ConsoleUpdateAsync exception: ") + e.what() + "\n",
			kConTextColorError, Channel::Console
		);
	} catch (...) {
		Print(
			"ConsoleUpdateAsync exception: Unknown error\n",
			kConTextColorError, Channel::Console
		);
	}
}

/// @brief コンソールスレッドを開始します
void Console::StartConsoleThread() {
	mConsoleThread = std::thread(&Console::ConsoleUpdateAsync, this);

	// スレッドのハンドルを取得
	HANDLE hThread = mConsoleThread.native_handle();

	HMODULE hKernel32            = GetModuleHandleW(L"kernel32.dll");
	auto    setThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(
		GetProcAddress(hKernel32, "SetThreadDescription")
	);

	// スレッドの説明を設定
	if (setThreadDescription) {
		setThreadDescription(hThread, L"ConsoleThread");
	} else {
		Print(
			"SetThreadDescription関数が見つかりませんでした。\n", kConTextColorError,
			Channel::Console
		);
	}
}

/// @brief メッセージを非同期でファイルにログします
/// @param message ログするメッセージ文字列
void Console::LogToFileAsync([[maybe_unused]] const std::string& message) {
#ifdef _DEBUG
	{
		std::lock_guard lock(mMutex);
		mMessageBuffer.emplace_back(message);

		// バッファが一定数を超えた場合はフラッシュ
		if (mMessageBuffer.size() >= kBatchSize) {
			std::vector<std::string> currentBatch = std::move(mMessageBuffer);
			mMessageBuffer                        = std::vector<std::string>();

			mTaskQueue.emplace(
				[currentBatch] { FlushLogBuffer(currentBatch); }
			);
			mCv.notify_one();
		}
	}
#endif
}

uint64_t                          Console::mFrameCount = 0;
std::mutex                        Console::mMutex;
std::queue<std::function<void()>> Console::mTaskQueue;
std::condition_variable           Console::mCv;

std::thread Console::mConsoleThread;
bool        Console::mStopThread = false;

#ifdef _DEBUG
bool                       Console::mShowConsole          = true;
bool                       Console::mWishScrollToBottom   = false;
bool                       Console::mShowSuggestPopup     = false;
bool                       Console::mShowAbout            = false;
bool                       Console::mFocusedConsoleWindow = false;
std::vector<Console::Text> Console::mConsoleTexts;
char                       Console::mInputText[kInputBufferSize] = {};
int                        Console::mHistoryIndex                = -1;
std::vector<std::string>   Console::mHistory;
std::vector<std::string>   Console::mSuggestions;
std::vector<uint64_t>      Console::mRepeatCounts;
int                        Console::mLastSelectedIndex    = -1;
Channel                    Console::mCurrentFilterChannel = Channel::None;

std::vector<std::string> Console::mMessageBuffer;
std::ofstream            Console::mLogFile;

Console::DisplayState Console::mDisplayState;

// ConVarヘルパー
bool                       Console::mShowConVarHelper    = false;
bool                       Console::mShowElementPopup    = false;
size_t                     Console::mEditingElementIndex = 0;
std::vector<Console::Page> Console::mPages               = {
	{
		.name     = "User Page 0",
		.grid     = {.width = 1, .height = 10},
		.elements = {
			GridElement{
				.type    = GridElement::Type::Label,
				.label   = "Player",
				.bgColor = kConHelperColorLabelBg,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Button,
				.label   = "Strafe",
				.command = "sv_airaccelerate 1000000000000",
				.bgColor = kConBgColorDark,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Button,
				.label   = "Reset Strafe",
				.command = "sv_airaccelerate 10",
				.bgColor = kConBgColorDark,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Label,
				.label   = "Debug",
				.bgColor = kConHelperColorLabelBg,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Button,
				.label   = "Show Entity Axis",
				.command = "toggle ent_axis",
				.bgColor = kConBgColorDark,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Button,
				.label   = "Show Console",
				.command = "toggleconsole",
				.bgColor = kConBgColorDark,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Button,
				.label   = "Toggle VSync",
				.command = "toggle r_vsync",
				.bgColor = kConBgColorDark,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Label,
				.label   = "World",
				.bgColor = kConHelperColorLabelBg,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Button,
				.label   = "No Gravity",
				.command = "sv_gravity 0",
				.bgColor = kConBgColorDark,
				.fgColor = kConFgColorDark,
			},
			GridElement{
				.type    = GridElement::Type::Button,
				.label   = "Reset Gravity",
				.command = "sv_gravity 800",
				.bgColor = kConBgColorDark,
				.fgColor = kConFgColorDark,
			},
		}
	},
};
size_t Console::mSelectedPageIndex = 0;
#endif
