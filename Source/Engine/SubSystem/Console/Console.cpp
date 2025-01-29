#include "Console.h"

#include <filesystem>
#include <format>
#include <fstream>

#include <ImGuiManager/Icons.h>

#include <Input/InputSystem.h>

#include <SubSystem/Console/ConCommand.h>
#include <SubSystem/Console/ConVarManager.h>
#include <Lib/Timer/EngineTimer.h>
#include <Lib/Utils/StrUtils.h>

#include <Window/WindowsUtils.h>

using SetThreadDescriptionFunc = HRESULT(WINAPI*)(HANDLE, PCWSTR);

Console::Console() {
	bStopThread_ = false;

#ifdef _DEBUG
	// ログファイルの初期化
	{
		std::lock_guard lock(mutex_);

		// 既存のログファイルを削除
		if (std::filesystem::exists("console.log")) {
			std::filesystem::remove("console.log");
		}

		// 新規ファイルを作成
		logFile_.open("console.log", std::ios::out | std::ios::binary);
		if (logFile_.is_open()) {
			// ヘッダーを書き込む
			const auto now = EngineTimer::GetNow();
			std::string header = std::format(
				"//-----------------------------------------------------------------------------\n"
				"// BuildDate: {}-{}\n"
				"// Engine: {} Ver.{}\n"
				"// LaunchDate: {:02}-{:02}-{:02} {:02}:{:02}:{:02}\n"
				"//-----------------------------------------------------------------------------\n\n",
				kEngineBuildDate, kEngineBuildTime,
				kEngineName, kEngineVersion,
				now.year, now.month, now.day, now.hour, now.minute, now.second
			);
			logFile_.write(header.c_str(), header.size());
			logFile_.flush();
		}
	}
#endif
	StartConsoleThread();

	ConCommand::RegisterCommand("toggleconsole", ToggleConsole, "Show/hide the console.");

	ConCommand::RegisterCommand("clear", Clear, "Clear all console output");
	ConCommand::RegisterCommand("cls", Clear, "Clear all console output");
	ConCommand::RegisterCommand("echo", Echo, "Echo text to console.");
	ConCommand::RegisterCommand("help", Help, "Find help about a convar/concommand.");
	ConCommand::RegisterCommand("neofetch", NeoFetch, "Show system info.");
	SubmitCommand("bind ` toggleconsole");
}

Console::~Console() {
	Shutdown();
}

void Console::Update() {
#ifdef _DEBUG
	if (!bShowConsole_) {
		return;
	}

	// ウィンドウフラグの設定
	ImGuiWindowFlags consoleWindowFlags =
		ImGuiWindowFlags_NoScrollbar | // スクロールバーを非表示
		ImGuiWindowFlags_NoScrollWithMouse | // マウスホイールでスクロールしない
		ImGuiWindowFlags_NoCollapse | // ウィンドウの折り畳みを無効
		ImGuiWindowFlags_MenuBar; // メニューバーを表示

	if (bShowSuggestPopup_) {
		// フォーカス時に前面に持ってこない
		consoleWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	// 最小サイズと最大サイズを設定
	ImGui::SetNextWindowSizeConstraints(
		{ 360.0f, 360.0f },
		{ 0xFFFF, 0xFFFF }
	);

	bool bWindowOpen = ImGui::Begin(
		(StrUtils::ConvertToUtf8(kIconTerminal) + " コンソール").c_str(), &bShowConsole_, consoleWindowFlags
	);

	if (bWindowOpen) {
		ShowMenuBar();

		bool bIsDarkMode = WindowsUtils::IsAppDarkTheme();

		if (!bIsDarkMode) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.2f, 0.2f, 0.2f, 1.0f });
		}

		if (ImGuiManager::IconButton(StrUtils::ConvertToUtf8(kIconTerminal).c_str(), "ConVar", { 48.0f,48.0f })) {
			bShowConVarHelper_ = !bShowConVarHelper_;
		}

		ImGui::Spacing();
		ImGui::Spacing();

		ShowConVarHelper();

		ShowConsoleBody();

		if (!bIsDarkMode) {
			ImGui::PopStyleColor();
		}

		if (bShowAbout_) {
			ShowAbout();
		}
	}
	ImGui::End();
#endif
}

void Console::Shutdown() {
	{
		std::lock_guard lock(mutex_);
		bStopThread_ = true;
	}

	// スレッドを再開
	cv_.notify_all();

	// スレッドが終了するまで待機
	if (consoleThread_.joinable()) {
		consoleThread_.join();
	}

#ifdef _DEBUG
	// 残りのメッセージをファイルに書き込む
	if (!messageBuffer_.empty()) {
		FlushLogBuffer(messageBuffer_);
		messageBuffer_.clear();
	}

	// ログファイルを閉じる
	if (logFile_.is_open()) {
		logFile_.close();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: コマンドを送信/実行します
//-----------------------------------------------------------------------------
void Console::SubmitCommand([[maybe_unused]] const std::string& command) {
	std::string trimmedCommand = TrimSpaces(command);

	// コマンドが空なのでなんもしない
	if (trimmedCommand.empty()) {
		Print(">\n", kConsoleColorNormal);
		return;
	}

	std::vector<std::string> tokens = TokenizeCommand(trimmedCommand);

	// とりあえず履歴に追加
	AddCommandHistory(trimmedCommand);

	bool found = ConCommand::ExecuteCommand(trimmedCommand);

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
		if (StrUtils::Equal(conVar->GetName(), tokens[0])) {
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
					kConsoleColorWarning
				);
				// 変数の説明を取得
				std::string description = conVar->GetHelp();
				// 変数の型を取得
				std::string type = std::format("[{}]\n", conVar->GetTypeAsString());

				Print(" - " + description + " " + type, GetConVarTypeColor(conVar->GetTypeAsString()));
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
							std::string vec3Value = std::format("{} {} {}", x, y, z);
							conVar->SetValueFromString(vec3Value); // ここで値を設定
						} catch (...) {
							isValidInput = false;
						}
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
								[[maybe_unused]] int value = std::stoi(tokens[i]);
							} catch (...) {
								isValidInput = false;
								break;
							}
						} else if (conVar->GetTypeAsString() == "float") {
							try {
								[[maybe_unused]] float value = std::stof(tokens[i]);
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
								if (tokens[i] == "true" || tokens[i] == "false") {
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
					Print("CVAR型変換エラー: 指定された型が無効です。\n", kConsoleColorError, Channel::Console);
					Print(
						"期待される型: " + conVar->GetTypeAsString() + "\n", GetConVarTypeColor(conVar->GetTypeAsString()),
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

#ifdef _DEBUG
	bWishScrollToBottom_ = true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: コンソールにテキストを出力します
// - message (std::string): 出力するテキスト
// - color (Vec4): テキストの色
// - channel (Channel): チャンネル
// Return: void
//-----------------------------------------------------------------------------
void Console::Print(
	[[maybe_unused]] const std::string& message, [[maybe_unused]] const Vec4& color,
	[[maybe_unused]] const Channel& channel
) {
	if (message.empty()) {
		return;
	}

	// ログへの書き込み
	if (ConVarManager::GetConVar("verbose")->GetValueAsBool()) {
		std::string channelStr = (channel != Channel::None) ? "[" + ToString(channel) + "] " : "";
		OutputDebugString(StrUtils::ToString(channelStr + message));
		LogToFileAsync(channelStr + message);
	}

	// タスクキューに追加
	{
		std::lock_guard lock(mutex_);
		taskQueue_.emplace([message, color, channel] {
			std::string msg = message;
			const bool hasNewLine = !msg.empty() && msg.back() == '\n';
			const std::string baseMsg = hasNewLine ? msg.substr(0, msg.size() - 1) : msg;

#ifdef _DEBUG
			if (!consoleTexts_.empty()) {
				const std::string lastMsg = consoleTexts_.back().text;
				const bool lastHasNewLine = !lastMsg.empty() && lastMsg.back() == '\n';
				const std::string lastBaseMsg = lastHasNewLine ? lastMsg.substr(0, lastMsg.size() - 1) : lastMsg;

				const size_t bracketPos = lastBaseMsg.find(" [x");
				const std::string lastBaseMsgWithoutCount =
					(bracketPos != std::string::npos) ? lastBaseMsg.substr(0, bracketPos) : lastBaseMsg;

				if (baseMsg == lastBaseMsgWithoutCount && lastMsg != "]\n") {
					UpdateRepeatCount(baseMsg, hasNewLine, channel, color);
					return;
				}
			}

			consoleTexts_.push_back({ .text = msg, .color = color, .channel = channel });
			repeatCounts_.push_back(1);
			bWishScrollToBottom_ = true;
#endif
			});
	}
	cv_.notify_one();
}

//-----------------------------------------------------------------------------
// Purpose: 開発者を叱ります
//-----------------------------------------------------------------------------
void Console::PrintNullptr(const std::string& message, const Channel& channel) {
	Print("ぬるぽ >> " + message + "\n", kConsoleColorError, channel);
	Print("ｶﾞｯ\n", kConsoleColorWarning, channel);
}

//-----------------------------------------------------------------------------
// Purpose: チャンネルを文字列に変換します
// - channel (Channel): チャンネル
//-----------------------------------------------------------------------------
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

//-----------------------------------------------------------------------------
// Purpose: コンソールの表示/非表示を切り替えます
//-----------------------------------------------------------------------------
void Console::ToggleConsole([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	bShowConsole_ = !bShowConsole_;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: すべてのコンソール出力をクリアします
//-----------------------------------------------------------------------------
void Console::Clear([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	consoleTexts_.clear(); // コンソールのテキストをクリア
	consoleTexts_.shrink_to_fit(); // 開放
	history_.clear(); // コマンド履歴をクリア
	history_.shrink_to_fit();
	suggestions_.clear(); // サジェストをクリア
	suggestions_.shrink_to_fit();
	repeatCounts_.clear(); // 繰り返しカウントをクリア
	repeatCounts_.shrink_to_fit();
	historyIndex_ = -1; // 履歴インデックスを初期化
	bWishScrollToBottom_ = true; // 再描画の際にスクロールをリセット
#endif
}

//-----------------------------------------------------------------------------
// Purpose: ヘルプを表示します
//-----------------------------------------------------------------------------
void Console::Help([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	ConCommand::Help();
	for (auto conVar : ConVarManager::GetAllConVars()) {
		Print(" - " + conVar->GetName() + " : " + conVar->GetHelp() + "\n", kConsoleColorNormal, Channel::None);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: システム情報を表示します
//-----------------------------------------------------------------------------
void Console::NeoFetch([[maybe_unused]] const std::vector<std::string>& args) {
	std::vector<std::string> asciiArt = {
		"                  >>>>>>>>                   ",
		"                    >>>>>>>>                 ",
		"                       >>>>>>>>              ",
		"                         >>>>>>>>            ",
		"                            >>>>>>>>         ",
		"                              >>>>>>>>       ",
		"  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    ",
		"  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>  ",
		"  >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>    ",
		"                              >>>>>>>>       ",
		"                            >>>>>>>>         ",
		"                         >>>>>>>>            ",
		"                       >>>>>>>>              ",
		"                    >>>>>>>>                 ",
		"                  >>>>>>>>                   ",
		"                                             "
	};

	const std::string prompt = WindowsUtils::GetWindowsUserName() + "@" + WindowsUtils::GetWindowsComputerName();

	const DateTime dateTime = EngineTimer::GetUpDateTime();

	std::vector<std::string> uptimeParts;
	if (dateTime.day > 0) {
		uptimeParts.push_back(std::to_string(dateTime.day) + " days,");
	}
	if (dateTime.hour > 0) {
		uptimeParts.push_back(std::to_string(dateTime.hour) + " hours,");
	}
	if (dateTime.minute > 0) {
		uptimeParts.push_back(std::to_string(dateTime.minute) + " mins,");
	}
	if (dateTime.second > 0) {
		uptimeParts.push_back(std::to_string(dateTime.second) + " sec");
	}
	const std::string uptime = "Uptime:  " + StrUtils::Join(uptimeParts, " ");

	const std::vector<std::string> info = {
		prompt + "\n",
		(!prompt.empty() ? std::string(prompt.size(), '-') : "") + "\n",
		uptime + "\n",
		"Resolution:  " + std::to_string(Window::GetClientWidth()) + "x" + std::to_string(Window::GetClientHeight()) +
		"\n",
		"CPU:  " + WindowsUtils::GetCPUName() + "\n",
		"GPU:  " + WindowsUtils::GetGPUName() + "\n",
		"Memory:  " + WindowsUtils::GetRamUsage() + " / " + WindowsUtils::GetRamMax() + "\n",
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
	const size_t maxRows = max(asciiArt.size(), info.size());
	const size_t asciiWidth = asciiArt[0].size();

	// 結合処理
	for (size_t i = 0; i < maxRows; ++i) {
		constexpr size_t padding = 5;
		std::string leftPart = (i < asciiArt.size()) ? asciiArt[i] : std::string(asciiWidth, ' ');
		std::string rightPart = (i < info.size()) ? info[i] : "";
		combined.push_back(leftPart + std::string(padding, ' ') + rightPart);
	}

	// 結合結果を一行ずつ出力
	for (const std::string& line : combined) {
		Print(line, kConsoleColorWait, Channel::None);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 入力された文字列をエコーします
//-----------------------------------------------------------------------------
void Console::Echo(const std::vector<std::string>& args) {
	Print(StrUtils::Join(args, " ") + "\n", kConsoleColorNormal, Channel::Console);
}

#ifdef _DEBUG
void Console::UpdateSuggestions(const std::string& input) {
	suggestions_.clear();

	// 入力が空の場合はサジェストしない
	if (input.empty()) {
		for (const auto& command : ConCommand::GetCommands()) {
			suggestions_.push_back(command.first);
		}
		for (const auto& conVar : ConVarManager::GetAllConVars()) {
			suggestions_.push_back(conVar->GetName());
		}
		return;
	}

	for (const auto& command : ConCommand::GetCommands()) {
		if (command.first.starts_with(input)) {
			suggestions_.push_back(command.first);
		}
	}

	for (const auto& conVar : ConVarManager::GetAllConVars()) {
		if (conVar->GetName().starts_with(input)) {
			suggestions_.push_back(conVar->GetName());
		}
	}
}
#endif

#ifdef _DEBUG
//-----------------------------------------------------------------------------
// Purpose: サジェストポップアップを表示します
//-----------------------------------------------------------------------------
void Console::ShowSuggestPopup() {
	if (suggestions_.empty()) {
		return;
	}

	// ポップアップの位置とサイズを計算
	ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
	ImVec2 inputTextSize = ImGui::GetItemRectSize();
	auto popupPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + inputTextSize.y);
	auto popupSize = ImVec2(
		inputTextSize.x,
		ImGui::GetTextLineHeight() * min(suggestions_.size(), size_t(10)) + ImGui::GetStyle().WindowPadding.y * 2
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
		for (size_t i = 0; i < suggestions_.size(); i++) {
			if (ImGui::Selectable(suggestions_[i].c_str())) {
				// 選択されたサジェストで入力を置き換え
				strncpy_s(inputText_, suggestions_[i].c_str(), sizeof(inputText_));
				bShowSuggestPopup_ = false;
			}
		}
	}
	ImGui::End();
}
#endif

#ifdef _DEBUG
void Console::SuggestPopup(
	[[maybe_unused]] SuggestPopupState& state, const ImVec2& pos, const ImVec2& size, [[maybe_unused]] bool& isFocused
) {
	// 角丸をなくす
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);

	ImGuiWindowFlags flags =
		ImGuiWindowFlags_NoTitleBar | // タイトルバーなし
		ImGuiWindowFlags_NoResize | // リサイズしない
		ImGuiWindowFlags_NoMove | // 移動しない
		ImGuiWindowFlags_HorizontalScrollbar | // 水平スクロールバー
		ImGuiWindowFlags_NoSavedSettings; // iniファイルに書き込まない

	ImGui::SetNextWindowPos(pos);
	ImGui::SetNextWindowSize(size);
	ImGui::Begin("suggestPopup", nullptr, flags);
	ImGui::PushAllowKeyboardFocus(false);

	for (size_t i = 0; i < kConsoleSuggestLineCount; ++i) {
		size_t test = ConVarManager::GetAllConVars().size();
		if (test <= i) {
			break;
		}

		bool isIndexActive = state.activeIndex == static_cast<int>(i);
		if (isIndexActive) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 0, 0, 1));
		}

		ImGui::PushID(static_cast<int>(i));
		if (ImGui::Selectable(ConVarManager::GetAllConVars()[i]->GetName().c_str(), isIndexActive)) {
			state.clickedIndex = static_cast<int>(i);
		}
		ImGui::PopID();

		if (isIndexActive) {
			ImGui::PopStyleColor();
		}
	}

	ImGui::PopAllowKeyboardFocus();
	ImGui::PopStyleVar();
	ImGui::End();
}
#endif

#ifdef _DEBUG
//-----------------------------------------------------------------------------
// Purpose: InputTextからのイベントを処理します。
//-----------------------------------------------------------------------------
int Console::InputTextCallback(ImGuiInputTextCallbackData* data) {
	switch (data->EventFlag) {
	case ImGuiInputTextFlags_CallbackCompletion:
		{
			// bShowSuggestPopup_ = !bShowSuggestPopup_;
			if (bShowSuggestPopup_) {
				UpdateSuggestions(data->Buf);
			}
		}
		break;

	case ImGuiInputTextFlags_CallbackHistory:
		{
			const int prev_history_index = historyIndex_;
			if (data->EventKey == ImGuiKey_UpArrow) {
				if (historyIndex_ > 0) {
					historyIndex_--;
				}
			} else if (data->EventKey == ImGuiKey_DownArrow) {
				if (historyIndex_ < static_cast<int>(history_.size()) - 1) {
					historyIndex_++;
				} else {
					historyIndex_ = static_cast<int>(history_.size()); // 履歴が空の場合はサイズと一致させる
				}
			}
			if (prev_history_index != historyIndex_) {
				data->DeleteChars(0, data->BufTextLen);
				if (historyIndex_ < static_cast<int>(history_.size())) {
					data->InsertChars(0, history_[historyIndex_].c_str());
				} else {
					data->InsertChars(0, ""); // 履歴が空の場合は空白を挿入
				}
			}
		}
		break;

	case ImGuiInputTextFlags_CallbackEdit: Print("Edit\n", kConsoleColorInt);
		break;

	case ImGuiInputTextFlags_CallbackResize: Print("Resize\n", kConsoleColorError);
		break;
	default:;
	}
	return 0;
}
#endif

//-----------------------------------------------------------------------------
// Purpose: メニューバーを表示します
//-----------------------------------------------------------------------------
void Console::ShowMenuBar() {
#ifdef _DEBUG
	if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("File")) {
			if (ImGui::MenuItem("Clear", "Ctrl+C")) {
				Clear({});
			}
			if (ImGui::MenuItem("Close", "Ctrl+W")) {
				bShowConsole_ = false;
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Help")) {
			if (ImGui::MenuItem("Help", "F1")) {
				Help({});
			}
			if (ImGui::MenuItem((StrUtils::ConvertToUtf8(0xeb8e) + " About Console").c_str())) {
				bShowAbout_ = true;
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: コンソールのテキストを表示します
//-----------------------------------------------------------------------------
void Console::ShowConsoleText() {
#ifdef _DEBUG
	std::lock_guard lock(mutex_);

	// 60FPSを超えない頻度でバッファを更新
	const size_t currentFrame = EngineTimer::GetFrameCount();
	if (currentFrame - displayState_.lastUpdateFrame > 1) {
		displayState_.buffer = consoleTexts_;
		displayState_.selected.resize(displayState_.buffer.size(), false);
		displayState_.lastUpdateFrame = currentFrame;
	}

	// 入力フィールドとボタンの高さを取得
	float inputTextHeight = ImGui::GetFrameHeightWithSpacing();
	// 子ウィンドウの高さを調整
	auto childSize = ImVec2(0, -inputTextHeight - ImGui::GetStyle().ItemSpacing.y);

	if (ImGui::BeginTable(
		"ConsoleTable", 2,
		ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit,
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
		for (size_t i = 0; i < displayState_.buffer.size(); ++i) {
			// チャンネルによるフィルター処理
			if (currentFilterChannel_ != Channel::None && displayState_.buffer[i].channel != currentFilterChannel_) {
				continue; // フィルターに一致しない場合はスキップ
			}

			ImGui::TableNextRow();

			// チャンネル列
			if (displayState_.buffer[i].channel != Channel::None) {
				if (ImGui::TableSetColumnIndex(0)) {
					std::string channelName = ToString(displayState_.buffer[i].channel);
					if (!channelName.empty()) {
						if (ImGui::TextLink((channelName + "##" + std::to_string(i)).c_str())) {
							// チャンネルをクリックした時のフィルター設定
							if (currentFilterChannel_ == displayState_.buffer[i].channel) {
								currentFilterChannel_ = Channel::None; // 同じチャンネルをクリックするとフィルター解除
							} else {
								currentFilterChannel_ = displayState_.buffer[i].channel;
							}
						}
					}
				}
			} else {
				ImGui::TableSetColumnIndex(0);
			}

			// ログ列
			if (ImGui::TableSetColumnIndex(1)) {
				ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(displayState_.buffer[i].color));
				bool isSelected = displayState_.selected[i];
				if (ImGui::Selectable((displayState_.buffer[i].text + "##" + std::to_string(i)).c_str(), isSelected)) {
					if (ImGui::GetIO().KeyCtrl) {
						// 選択状態のトグル
						displayState_.selected[i] = !displayState_.selected[i];
					} else if (ImGui::GetIO().KeyShift && lastSelectedIndex_ != -1) {
						// フィルタリング後の範囲選択
						const int start = min(lastSelectedIndex_, visibleIndex);
						const int end = max(lastSelectedIndex_, visibleIndex);
						for (int j = start; j <= end; ++j) {
							size_t actualIndex = FilteredToActualIndex(j);
							if (actualIndex != SIZE_MAX) {
								displayState_.selected[actualIndex] = true;
							}
						}
					} else {
						// 単一選択（フィルタリング後の要素に限定）
						std::fill(displayState_.selected.begin(), displayState_.selected.end(), false);
						displayState_.selected[i] = true;
					}
					lastSelectedIndex_ = visibleIndex; // フィルタリング後のインデックス
				}
				ImGui::PopStyleColor();
			}
			visibleIndex++; // フィルタリング後のインデックスを更新
		}

		ShowContextMenu();

		// Ctrl+C でコピー
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
			std::string copiedText;
			for (size_t i = 0; i < displayState_.buffer.size(); ++i) {
				if (displayState_.selected[i]) {
					copiedText += displayState_.buffer[i].text;
				}
			}
			ImGui::SetClipboardText(copiedText.c_str());
		}

		CheckScroll();

		ImGui::EndTable();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: コンソールの本体を表示します
//-----------------------------------------------------------------------------
void Console::ShowConsoleBody() {
#ifdef _DEBUG
	ImGui::PushStyleColor(ImGuiCol_Border, { 0.0f, 0.0f, 0.0f, 0.0f });
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, { 0.0f, 0.0f, 0.0f, 0.0f });

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 3.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 16.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 128.0f);

	ShowConsoleText();

	ImGui::Spacing();

	ImVec2 size = ImGui::GetContentRegionAvail();
	const float buttonWidth = ImGui::CalcTextSize(" Submit ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
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

	if (ImGui::InputText("##input", inputText_, IM_ARRAYSIZE(inputText_), inputTextFlags, InputTextCallback)) {
		ImGui::SetKeyboardFocusHere(-1);
		SubmitCommand(inputText_);
		if (!std::string(inputText_).empty()) {
			history_.emplace_back(inputText_);
			historyIndex_ = static_cast<int>(history_.size());
		}
		memset(inputText_, 0, sizeof inputText_);
	}

	ShowSuggestPopup();

	ImGui::SameLine();

	if (ImGui::Button(" Submit ")) {
		SubmitCommand(inputText_);
		if (!std::string(inputText_).empty()) {
			history_.emplace_back(inputText_);
			historyIndex_ = static_cast<int>(history_.size());
		}
		memset(inputText_, 0, sizeof inputText_);
	}

	ImGui::PopStyleVar(3);
	ImGui::PopStyleColor(2);
#endif
}

void Console::ShowContextMenu() {
#ifdef _DEBUG
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
	ImGui::PushStyleColor(ImGuiCol_Border, { 0.25f, 0.25f, 0.25f, 1.0f });

	// 選択された全テキストをコピーする機能（右クリックメニューから）
	if (ImGui::BeginPopupContextWindow()) {
		ImGui::Spacing();

		if (ImGui::MenuItem((StrUtils::ConvertToUtf8(kIconCopy) + " Copy Selected").c_str())) {
			std::string copiedText;
			for (size_t i = 0; i < displayState_.buffer.size(); ++i) {
				if (displayState_.selected[i]) {
					copiedText += displayState_.buffer[i].text;
				}
			}
			ImGui::SetClipboardText(copiedText.c_str());
		}

		ImGui::Separator();

		if (ImGui::MenuItem((StrUtils::ConvertToUtf8(kIconVisibility) + " Show All Channels").c_str())) {
			currentFilterChannel_ = Channel::None;
		}

		ImGui::Spacing();
		ImGui::EndPopup();
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
#endif
}

void Console::ShowAbout() {
#ifdef _DEBUG
	ImGui::OpenPopup(("About " + kEngineName + " Console").c_str());

	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize({ 280.0f, 230.0f }, ImGuiCond_FirstUseEver);

	const bool bOpen = ImGui::BeginPopupModal(
		("About " + kEngineName + " Console").c_str(),
		&bShowAbout_,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoSavedSettings
	);

	if (bOpen) {
		ImGui::Text((StrUtils::ConvertToUtf8(kIconTerminal) + " Unnamed Console").c_str());
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Version: %s", kEngineVersion.c_str());
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("Build: %s %s", kEngineBuildDate.c_str(), kEngineBuildTime.c_str());
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Text("ImGui Version: %s", ImGui::GetVersion());
		ImGui::Spacing();
		ImGui::Spacing();
		// ボタンサイズとウィンドウサイズを取得
		auto buttonSize = ImVec2(74.0f, 24.0f);
		ImVec2 windowSize = ImGui::GetWindowSize(); // ウィンドウ全体のサイズ
		ImVec2 cursorStartPos = ImGui::GetCursorPos(); // 現在のカーソル位置

		// ボタンを中央に配置
		float buttonPosX = (windowSize.x - buttonSize.x) * 0.5f; // 中央揃えのX座標
		float buttonPosY = cursorStartPos.y + ImGui::GetContentRegionAvail().y - buttonSize.y; // 下端からの位置調整
		ImGui::SetCursorPos(ImVec2(buttonPosX, buttonPosY));

		if (ImGui::Button("OK", buttonSize)) {
			bShowAbout_ = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
#endif
}

void Console::ShowConVarHelper() {
#ifdef _DEBUG
	if (!bShowConVarHelper_) {
		return;
	}

	bool isOpen = ImGui::Begin("ConVar Helper", &bShowConVarHelper_);

	if (isOpen) {
		// ページ選択
		{
			// ComboBoxの表示
			ImGui::Text("Page");
			ImGui::SameLine();
			if (ImGui::BeginCombo("##Page", pages_[selectedPageIndex_].name.c_str(), ImGuiComboFlags_WidthFitPreview)) {
				for (uint32_t n = 0; n < pages_.size(); n++) {
					const bool isSelected = (static_cast<int>(selectedPageIndex_) == n);
					if (ImGui::Selectable(pages_[n].name.c_str(), isSelected)) {
						selectedPageIndex_ = n;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}
				ImGui::EndCombo();
			}

			ImGui::SameLine();

			if (ImGui::Button("+", { 24,0 })) {
				Page newPage = {
					.name = "User Page " + std::to_string(pages_.size()),
					.grid = { 3, 10 },
				};
				pages_.push_back(newPage);
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
			strncpy_s(pageNameBuffer, pages_[selectedPageIndex_].name.c_str(), sizeof(pageNameBuffer));
			if (ImGui::InputText("PageName", pageNameBuffer, sizeof(pageNameBuffer))) {
				pages_[selectedPageIndex_].name = pageNameBuffer;
			}
		}

		// グリッドの設定
		{
			// ウィンドウの利用可能な幅を取得
			const float windowWidth = ImGui::GetContentRegionAvail().x;

			// 各要素の幅を計算
			const float labelWidth = ImGui::CalcTextSize("Grid").x;
			const float xTextWidth = ImGui::CalcTextSize("x").x;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float inputWidth = (windowWidth - labelWidth - xTextWidth - spacing * 4) * 0.5f;

			ImGui::Text("Grid");
			ImGui::SameLine();

			ImGui::SetNextItemWidth(inputWidth);
			int tmpWidth = static_cast<int>(pages_[selectedPageIndex_].grid.width);
			if (ImGui::InputInt("##Width", &tmpWidth, 1)) {
				pages_[selectedPageIndex_].grid.width = max(1, tmpWidth);
			}

			ImGui::SameLine();
			ImGui::Text("x");
			ImGui::SameLine();

			ImGui::SetNextItemWidth(inputWidth);
			int tmpHeight = static_cast<int>(pages_[selectedPageIndex_].grid.height);
			if (ImGui::InputInt("##Height", &tmpHeight, 1)) {
				pages_[selectedPageIndex_].grid.height = max(1, tmpHeight);
			}
		}

		// グリッドサイズのテーブルを作成
		{
			const auto& grid = pages_[selectedPageIndex_].grid;

			// グリッド要素の数を正しく保つ
			size_t expectedSize = grid.width * grid.height;
			auto& elements = pages_[selectedPageIndex_].elements;
			if (elements.size() != expectedSize) {
				elements.resize(expectedSize);
			}

			if (ImGui::BeginTable("GridTable", grid.width,
				ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
				ImGuiTableFlags_SizingFixedFit |
				ImGuiTableFlags_NoPadInnerX |
				ImGuiTableFlags_NoPadOuterX)) {

				// 列幅を均等に設定
				float columnWidth = ImGui::GetContentRegionAvail().x / static_cast<float>(grid.width);
				for (uint32_t i = 0; i < grid.width; ++i) {
					ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, columnWidth);
				}

				// スタイルのパディングとスペーシングをなくす
				ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(0, 0));
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

				// 固定のセル高さを設定
				const float cellHeight = 50.0f;

				for (uint32_t row = 0; row < grid.height; ++row) {
					ImGui::TableNextRow(0, cellHeight);
					for (uint32_t col = 0; col < grid.width; ++col) {
						ImGui::TableSetColumnIndex(col);

						// セルのインデックスを計算
						size_t cellIndex = row * grid.width + col;
						GridElement& element = elements[cellIndex];

						// セルの内容を表示
						std::string cellId = std::format("Cell ({}, {})", col, row);

						// ボタンのサイズをセルの幅と高さに合わせる
						ImVec2 cellSize = ImVec2(columnWidth, cellHeight);

						if (element.type == GridElement::Type::None) {
							// 要素がない場合は見えないボタンを配置してコンテキストメニューを有効にする
							ImGui::InvisibleButton(cellId.c_str(), cellSize);

							// 右クリックでコンテキストメニューを表示
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								if (ImGui::MenuItem("Add Label")) {
									bShowElementPopup_ = true;
									editingElementIndex_ = cellIndex;
									element.type = GridElement::Type::Label;
								}
								if (ImGui::MenuItem("Add Button")) {
									bShowElementPopup_ = true;
									editingElementIndex_ = cellIndex;
									element.type = GridElement::Type::Button;
								}
								ImGui::EndPopup();
							}
						} else if (element.type == GridElement::Type::Label) {
							// ラベルの場合は、テキストを表示
							ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(element.color));

							// テキストを中央揃えで配置
							ImVec2 textSize = ImGui::CalcTextSize(element.label.c_str());
							ImVec2 pos = ImGui::GetCursorPos();
							pos.x += (cellSize.x - textSize.x) * 0.5f;
							pos.y += (cellSize.y - textSize.y) * 0.5f;
							ImGui::SetCursorPos(pos);

							ImGui::TextUnformatted(element.label.c_str());
							ImGui::PopStyleColor();

							// 右クリックでコンテキストメニューを表示
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								if (ImGui::MenuItem("Edit Label")) {
									bShowElementPopup_ = true;
									editingElementIndex_ = cellIndex;
								}
								if (ImGui::MenuItem("Remove Element")) {
									element = GridElement(); // 要素を削除
								}
								ImGui::EndPopup();
							}
						} else if (element.type == GridElement::Type::Button) {
							// ボタンの場合は、ボタンを表示
							ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(element.color));

							if (ImGui::Button(element.label.c_str(), cellSize)) {
								// ボタンのクリック処理
								SubmitCommand(element.command);
							}

							ImGui::PopStyleColor();

							// 右クリックでコンテキストメニューを表示
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								if (ImGui::MenuItem("Edit Button")) {
									bShowElementPopup_ = true;
									editingElementIndex_ = cellIndex;
								}
								if (ImGui::MenuItem("Remove Element")) {
									element = GridElement(); // 要素を削除
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

void Console::ShowElementEditPopup() {
#ifdef _DEBUG
	if (!bShowElementPopup_) {
		return;
	}

	// ポップアップ名を "Edit Element" に統一
	ImGui::OpenPopup("Edit Element");

	// フルスクリーンポップアップの設定
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;

	// 画面中心に表示
	ImVec2 size = ImVec2(400, 200);
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(size);

	// ポップアップ名を "Edit Element" に統一
	if (ImGui::BeginPopupModal("Edit Element", &bShowElementPopup_, flags)) {
		auto& element = pages_[selectedPageIndex_].elements[editingElementIndex_];

		if (element.type == GridElement::Type::Label) {
			char labelBuffer[256];
			strncpy_s(labelBuffer, element.label.c_str(), sizeof(labelBuffer));
			if (ImGui::InputText("Label", labelBuffer, sizeof(labelBuffer))) {
				element.label = labelBuffer;
			}
			ImGui::ColorEdit4("Color", &element.color.x);
		} else if (element.type == GridElement::Type::Button) {
			char commandBuffer[256];
			strncpy_s(commandBuffer, element.command.c_str(), sizeof(commandBuffer));
			if (ImGui::InputText("ConVar/ConCommand", commandBuffer, sizeof(commandBuffer))) {
				element.command = commandBuffer;
			}

			char labelBuffer[256];
			strncpy_s(labelBuffer, element.label.c_str(), sizeof(labelBuffer));
			if (ImGui::InputText("Label", labelBuffer, sizeof(labelBuffer))) {
				element.label = labelBuffer;
			}

			ImGui::ColorEdit4("Color", &element.color.x);
		}

		// OK と Cancel ボタン
		if (ImGui::Button("OK")) {
			bShowElementPopup_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			bShowElementPopup_ = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: InputSystemにコマンドを送信します
// - command (std::string): 送信するコマンド
// Return: コマンドが有効か?
//-----------------------------------------------------------------------------
bool Console::ProcessInputCommand(const std::string& command) {
	if (command.empty() || (command[0] != '+' && command[0] != '-')) {
		return false;
	}

	InputSystem::ExecuteCommand(command, command[0] == '+');
	return true;
}

//-----------------------------------------------------------------------------
// Purpose: 型が一致しているか確認します
// - value (std::string): 確認する値
// - type (std::string): 確認する型
// Return: 型が一致しているか?
//-----------------------------------------------------------------------------
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
	} catch (...) {
		return false;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 型変換エラーを表示します
// - type (std::string): エラーが発生した型
//-----------------------------------------------------------------------------
void Console::PrintTypeError(const std::string& type) {
	Print("CVAR型変換エラー: 指定された型が無効です。\n", kConsoleColorError, Channel::Console);
	Print("期待される型: " + type + "\n", GetConVarTypeColor(type), Channel::Console);
}

//-----------------------------------------------------------------------------
// Purpose: コマンド履歴に追加します
// - command (std::string): 履歴に追加するコマンド
//-----------------------------------------------------------------------------
void Console::AddCommandHistory([[maybe_unused]] const std::string& command) {
#ifdef _DEBUG
	consoleTexts_.push_back(
		{
			.text = "> " + command + "\n",
			.color = kConsoleColorExecute,
			.channel = Channel::Console
		}
	);
#endif
}

//-----------------------------------------------------------------------------
// Purpose: リピートカウントを更新
// - message (std::string): メッセージ
// - hasNewLine (bool): 改行が含まれているか
// - color (Vec4): テキストの色
//-----------------------------------------------------------------------------
void Console::UpdateRepeatCount(
	[[maybe_unused]] const std::string& message,
	[[maybe_unused]] const bool hasNewLine,
	[[maybe_unused]] const Channel& channel,
	[[maybe_unused]] const Vec4& color
) {
#ifdef _DEBUG
	repeatCounts_.back()++;

	const auto repeatCount = repeatCounts_.back();
	// 改行を含めない形式でメッセージを作成し、必要な場合のみ最後に追加
	auto formattedMessage = std::format(
		"{} [x{}]{}", message, repeatCount,
		hasNewLine ? "\n" : ""
	);

	if (repeatCount >= static_cast<int>(kConsoleRepeatError)) {
		consoleTexts_.back() = { .text = formattedMessage, .color = kConsoleColorError, .channel = channel };
	} else if (repeatCount >= static_cast<int>(kConsoleRepeatWarning)) {
		consoleTexts_.back() = { .text = formattedMessage, .color = kConsoleColorWarning, .channel = channel };
	} else {
		consoleTexts_.back() = { .text = formattedMessage, .color = color, .channel = channel };
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 最下部までスクロールされている場合は自動スクロールします
//-----------------------------------------------------------------------------
void Console::CheckScroll() {
#ifdef _DEBUG
	if (bWishScrollToBottom_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
		ImGui::SetScrollHereY(1.0f);
	}

	if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
		bWishScrollToBottom_ = false;
	} else {
		bWishScrollToBottom_ = true;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 行数をチェックして最大行数を超えた場合は削除します
//-----------------------------------------------------------------------------
void Console::CheckLineCountAsync() {
	std::lock_guard lock(mutex_);
#ifdef _DEBUG
	// コンソールテキスト用の初期キャパシティを設定
	constexpr size_t kInitialCapacity = 1024;
	constexpr size_t kGrowthFactor = 512;    // 増加量
	constexpr size_t kHistoryCapacity = 128;  // コマンド履歴の最大数
	constexpr size_t kSuggestionsInitialCapacity = 32;

	// コンソールテキストのキャパシティ管理
	if (consoleTexts_.capacity() < kInitialCapacity) {
		consoleTexts_.reserve(kInitialCapacity);
		repeatCounts_.reserve(kInitialCapacity);
	}

	// 履歴のキャパシティ管理
	if (history_.capacity() < kHistoryCapacity) {
		history_.reserve(kHistoryCapacity);
	}

	// サジェストのキャパシティ管理
	if (suggestions_.capacity() < kSuggestionsInitialCapacity) {
		suggestions_.reserve(kSuggestionsInitialCapacity);
	}

	// 最大行数を超えた場合は前方から削除
	while (consoleTexts_.size() > kConsoleMaxLineCount) {
		consoleTexts_.erase(consoleTexts_.begin());
		repeatCounts_.erase(repeatCounts_.begin());
	}

	// 履歴が最大数を超えた場合、古いものを削除
	if (history_.size() >= kHistoryCapacity) {
		history_.erase(history_.begin());
		historyIndex_ = static_cast<int>(history_.size()) - 1;
	}

	// コンソールテキストのキャパシティが大きすぎる場合は再確保
	if (consoleTexts_.capacity() > consoleTexts_.size() + kGrowthFactor * 2) {
		std::vector newTexts(consoleTexts_.begin(), consoleTexts_.end());
		std::vector newCounts(repeatCounts_.begin(), repeatCounts_.end());
		consoleTexts_ = std::move(newTexts);
		repeatCounts_ = std::move(newCounts);
	}
#endif
}

Vec4 Console::GetConVarTypeColor(const std::string& type) {
	if (type == "bool") {
		return kConsoleColorBool;
	}
	if (type == "int") {
		return kConsoleColorInt;
	}
	if (type == "float") {
		return kConsoleColorFloat;
	}
	if (type == "Vec3") {
		return kConsoleColorVec3;
	}
	if (type == "string") {
		return kConsoleColorString;
	}
	return kConsoleColorNormal;
}

//-----------------------------------------------------------------------------
// Purpose: 文字列の余分な空白を削除します
//-----------------------------------------------------------------------------
std::string Console::TrimSpaces(const std::string& string) {
	const size_t start = string.find_first_not_of(" \t\n\r");
	const size_t end = string.find_last_not_of(" \t\n\r");

	if (start == std::string::npos || end == std::string::npos) {
		return "";
	}

	return string.substr(start, end - start + 1);
}

//-----------------------------------------------------------------------------
// Purpose: コマンドをトークン化します
//-----------------------------------------------------------------------------
std::vector<std::string> Console::TokenizeCommand(const std::string& command) {
	std::istringstream stream(command);
	std::vector<std::string> tokens;
	std::string token;

	while (stream >> token) {
		tokens.push_back(token);
	}

	return tokens;
}

size_t Console::FilteredToActualIndex([[maybe_unused]] const int filteredIndex) {
#ifdef _DEBUG
	int visibleIndex = 0;
	for (size_t i = 0; i < displayState_.buffer.size(); ++i) {
		if (currentFilterChannel_ == Channel::None || displayState_.buffer[i].channel == currentFilterChannel_) {
			if (visibleIndex == filteredIndex) {
				return i;
			}
			visibleIndex++;
		}
	}
	return SIZE_MAX; // 該当なし
#endif
}

void Console::FlushLogBuffer([[maybe_unused]] const std::vector<std::string>& buffer) {
#ifdef _DEBUG
	if (!logFile_.is_open()) {
		logFile_.open("console.log", std::ios::app | std::ios::binary);
	}

	if (logFile_.is_open()) {
		for (const auto& msg : buffer) {
			logFile_ << msg;
		}
		logFile_.flush();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: コンソールスレッドを非同期で更新します
//-----------------------------------------------------------------------------
void Console::ConsoleUpdateAsync() {
	try {
		while (!bStopThread_) {
			std::vector<std::function<void()>> currentTasks;

			{
				std::unique_lock lock(mutex_);
				cv_.wait(lock, [this] { return !taskQueue_.empty() || bStopThread_; });

				if (bStopThread_) {
					break;
				}

				while (!taskQueue_.empty()) {
					currentTasks.push_back(std::move(taskQueue_.front()));
					taskQueue_.pop();
				}
			}

			for (auto& task : currentTasks) {
				try {
					task();
				} catch (const std::length_error& e) {
					Print(std::string("Length error in task: ") + e.what() + "\n", kConsoleColorError, Channel::Console);
				} catch (const std::exception& e) {
					Print(std::string("Task exception: ") + e.what() + "\n", kConsoleColorError, Channel::Console);
				} catch (...) {
					Print("Task exception: Unknown error\n", kConsoleColorError, Channel::Console);
				}
			}

			CheckLineCountAsync();
			std::this_thread::sleep_for(std::chrono::microseconds(100));
		}
	} catch (const std::exception& e) {
		Print(std::string("ConsoleUpdateAsync exception: ") + e.what() + "\n", kConsoleColorError, Channel::Console);
	} catch (...) {
		Print("ConsoleUpdateAsync exception: Unknown error\n", kConsoleColorError, Channel::Console);
	}
}

//-----------------------------------------------------------------------------
// Purpose: コンソールスレッドを開始します
//-----------------------------------------------------------------------------
void Console::StartConsoleThread() {
	consoleThread_ = std::thread(&Console::ConsoleUpdateAsync, this);

	// スレッドのハンドルを取得
	HANDLE hThread = consoleThread_.native_handle();

	HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
	auto setThreadDescription = reinterpret_cast<SetThreadDescriptionFunc>(
		GetProcAddress(hKernel32, "SetThreadDescription")
		);

	// スレッドの説明を設定
	if (setThreadDescription) {
		setThreadDescription(hThread, L"ConsoleThread");
	} else {
		Print("SetThreadDescription関数が見つかりませんでした。\n", kConsoleColorError, Channel::Console);
	}

}

//-----------------------------------------------------------------------------
// Purpose: 非同期でログをファイルに書き込みます
//-----------------------------------------------------------------------------
void Console::LogToFileAsync([[maybe_unused]] const std::string& message) {
#ifdef _DEBUG
	{
		std::lock_guard lock(mutex_);
		messageBuffer_.push_back(message);

		// バッファが一定数を超えた場合はフラッシュ
		if (messageBuffer_.size() >= kBatchSize) {
			std::vector<std::string> currentBatch = std::move(messageBuffer_);
			messageBuffer_ = std::vector<std::string>();

			taskQueue_.emplace([currentBatch] {
				FlushLogBuffer(currentBatch);
				});
			cv_.notify_one();
		}
	}
#endif
}

std::mutex Console::mutex_;
std::queue<std::function<void()>> Console::taskQueue_;
std::condition_variable Console::cv_;

std::thread Console::consoleThread_;
bool Console::bStopThread_ = false;

#ifdef _DEBUG
bool Console::bShowConsole_ = true;
bool Console::bWishScrollToBottom_ = false;
bool Console::bShowSuggestPopup_ = false;
bool Console::bShowAbout_ = false;
std::vector<Console::Text> Console::consoleTexts_;
char Console::inputText_[kInputBufferSize] = { 0 };
int Console::historyIndex_ = -1;
std::vector<std::string> Console::history_;
std::vector<std::string> Console::suggestions_;
std::vector<uint64_t> Console::repeatCounts_;
int Console::lastSelectedIndex_ = -1;
Channel Console::currentFilterChannel_ = Channel::None;

std::vector<std::string> Console::messageBuffer_;
std::ofstream Console::logFile_;

Console::DisplayState Console::displayState_;

// ConVarヘルパー
bool Console::bShowConVarHelper_ = false;
bool Console::bShowElementPopup_ = false;
size_t Console::editingElementIndex_ = 0;
std::vector<Console::Page> Console::pages_ = {
	{
		.name = "User Page 0",
		.grid = {.width = 3, .height = 10 }
	},
};
size_t Console::selectedPageIndex_ = 0;
#endif

