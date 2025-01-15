#include "Console.h"

#include <format>
#include <fstream>
#include <ImGuiManager/Icons.h>
#include <Input/InputSystem.h>
#include <Lib/Console/ConCommand.h>
#include <Lib/Console/ConVarManager.h>
#include <Lib/Timer/EngineTimer.h>
#include <Lib/Utils/StrUtils.h>
#include <Window/WindowsUtils.h>

//-----------------------------------------------------------------------------
// Purpose: コンソールを更新します
//-----------------------------------------------------------------------------
void Console::Update() {
#ifdef _DEBUG
	if (!bShowConsole_) {
		return;
	}

	ImGuiWindowFlags consoleWindowFlags = ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_MenuBar;

	if (bShowSuggestPopup_) {
		consoleWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	ImGui::SetNextWindowSizeConstraints({ 360.0f, 360.0f }, { 0xFFFF, 0xFFFF });

	bool bWindowOpen = ImGui::Begin((StrUtils::ConvertToUtf8(kIconTerminal) + " コンソール").c_str(), &bShowConsole_, consoleWindowFlags);

	if (bWindowOpen) {
		ShowMenuBar();

		bool bIsDarkMode = WindowsUtils::IsAppDarkTheme();

		if (!bIsDarkMode) {
			ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.2f,0.2f,0.2f,1.0f });
		}

		ShowConsoleBody();

		if (!bIsDarkMode) {
			ImGui::PopStyleColor();
		}

		if (bShowAbout_) {
			ShowAbout();
		}
	}
	ImGui::End();

	CheckLineCount();
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
					if (tokens.size() < 4) { // tokens[0]が変数名なので、1,2,3の引数を含めて最低4つ必要
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
					Print("期待される型: " + conVar->GetTypeAsString() + "\n", GetConVarTypeColor(conVar->GetTypeAsString()), Channel::Console);
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
#ifdef _DEBUG
	if (message.empty()) {
		return;
	}

	std::string msg = message;

	// 改行の有無をチェック
	const bool hasNewLine = !msg.empty() && msg.back() == '\n';
	const std::string baseMsg = hasNewLine ? msg.substr(0, msg.size() - 1) : msg;

	// 前のメッセージと完全一致するかチェック（改行を除いて比較）
	if (!consoleTexts_.empty()) {
		const std::string lastMsg = consoleTexts_.back().text;
		const bool lastHasNewLine = !lastMsg.empty() && lastMsg.back() == '\n';
		const std::string lastBaseMsg = lastHasNewLine ? lastMsg.substr(0, lastMsg.size() - 1) : lastMsg;

		// リピートカウント表示を除いた部分で比較
		const size_t bracketPos = lastBaseMsg.find(" [x");
		const std::string lastBaseMsgWithoutCount = (bracketPos != std::string::npos)
			? lastBaseMsg.substr(0, bracketPos)
			: lastBaseMsg;

		if (baseMsg == lastBaseMsgWithoutCount && lastMsg != "]\n") {
			UpdateRepeatCount(baseMsg, hasNewLine, color);
			return;
		}
	}

	// 新しいメッセージとして追加
	consoleTexts_.push_back({ .text = msg, .color = color, .channel = channel });
	repeatCounts_.push_back(1);
	// 内蔵コンソール以外はチャンネルを表示
	std::string channelStr = (channel != Channel::None) ? "[" + ToString(channel) + "] " : "";
	OutputDebugString(StrUtils::ToString(channelStr + msg));
	RewriteLogFile();

	bWishScrollToBottom_ = true;
#endif
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
	case Channel::None: return "None";
	case Channel::Console: return "Console";
	case Channel::Engine: return "Engine";
	case Channel::Host: return "Host";
	case Channel::ResourceManager: return "ResourceManager";
	case Channel::Client: return "Client";
	case Channel::Server: return "Server";
	case Channel::Game: return "Game";
	case Channel::InputSystem: return "InputSystem";
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
		if (command.first.find(input) == 0) {
			suggestions_.push_back(command.first);
		}
	}

	for (const auto& conVar : ConVarManager::GetAllConVars()) {
		if (conVar->GetName().find(input) == 0) {
			suggestions_.push_back(conVar->GetName());
		}
	}
}

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
	ImVec2 popupPos = ImVec2(cursorScreenPos.x, cursorScreenPos.y + inputTextSize.y);
	ImVec2 popupSize = ImVec2(inputTextSize.x, ImGui::GetTextLineHeight() * min(suggestions_.size(), size_t(10)) + ImGui::GetStyle().WindowPadding.y * 2);

	// ポップアップウィンドウの表示
	ImGui::SetNextWindowPos(popupPos);
	ImGui::SetNextWindowSize(popupSize);

	if (ImGui::Begin("Suggestions", nullptr,
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_Tooltip)) {

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

	case ImGuiInputTextFlags_CallbackEdit:
		Print("Edit\n", kConsoleColorInt);
		break;

	case ImGuiInputTextFlags_CallbackResize:
		Print("Resize\n", kConsoleColorError);
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
}

//-----------------------------------------------------------------------------
// Purpose: コンソールのテキストを表示します
//-----------------------------------------------------------------------------
void Console::ShowConsoleText() {
	// 入力フィールドとボタンの高さを取得
	float inputTextHeight = ImGui::GetFrameHeightWithSpacing();
	// 子ウィンドウの高さを調整
	ImVec2 childSize = ImVec2(0, -inputTextHeight - ImGui::GetStyle().ItemSpacing.y);

	if (ImGui::BeginTable("ConsoleTable", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX | ImGuiTableFlags_SizingFixedFit, childSize)) {
		// ヘッダー
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableSetupColumn("Log", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableHeadersRow();

		// フィルタリング後の行インデックスを追跡するための変数
		int visibleIndex = 0;

		// データ行
		for (size_t i = 0; i < consoleTexts_.size(); ++i) {
			// チャンネルによるフィルター処理
			if (currentFilterChannel_ != Channel::None && consoleTexts_[i].channel != currentFilterChannel_) {
				continue; // フィルターに一致しない場合はスキップ
			}

			ImGui::TableNextRow();

			// チャンネル列
			if (consoleTexts_[i].channel != Channel::None) {
				if (ImGui::TableSetColumnIndex(0)) {
					std::string channelName = ToString(consoleTexts_[i].channel);
					if (ImGui::TextLink((channelName + "##" + std::to_string(i)).c_str())) {
						// チャンネルをクリックした時のフィルター設定
						if (currentFilterChannel_ == consoleTexts_[i].channel) {
							currentFilterChannel_ = Channel::None; // 同じチャンネルをクリックするとフィルター解除
						} else {
							currentFilterChannel_ = consoleTexts_[i].channel;
						}
					}
				}
			} else {
				ImGui::TableSetColumnIndex(0);
			}

			// ログ列
			if (ImGui::TableSetColumnIndex(1)) {
				ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(consoleTexts_[i].color));
				bool isSelected = selectedItems_[i];
				if (ImGui::Selectable((consoleTexts_[i].text + "##" + std::to_string(i)).c_str(), isSelected)) {
					if (ImGui::GetIO().KeyCtrl) {
						// 選択状態のトグル
						selectedItems_[i] = !selectedItems_[i];
					} else if (ImGui::GetIO().KeyShift && lastSelectedIndex_ != -1) {
						// フィルタリング後の範囲選択
						const int start = min(lastSelectedIndex_, visibleIndex);
						const int end = max(lastSelectedIndex_, visibleIndex);
						for (int j = start; j <= end; ++j) {
							size_t actualIndex = FilteredToActualIndex(j);
							if (actualIndex != SIZE_MAX) {
								selectedItems_[actualIndex] = true;
							}
						}
					} else {
						// 単一選択（フィルタリング後の要素に限定）
						std::fill(selectedItems_.begin(), selectedItems_.end(), false);
						selectedItems_[i] = true;
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
			for (size_t i = 0; i < consoleTexts_.size(); ++i) {
				if (selectedItems_[i]) {
					copiedText += consoleTexts_[i].text;
				}
			}
			ImGui::SetClipboardText(copiedText.c_str());
		}

		CheckScroll();

		ImGui::EndTable();
	}
}

//-----------------------------------------------------------------------------
// Purpose: コンソールの本体を表示します
//-----------------------------------------------------------------------------
void Console::ShowConsoleBody() {
	ImGui::PushStyleColor(ImGuiCol_Border, { 0.0f,0.0f,0.0f,0.0f });
	ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, { 0.0f,0.0f,0.0f,0.0f });

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 3.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 16.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 128.0f);

	if (selectedItems_.size() != consoleTexts_.size()) {
		selectedItems_.resize(consoleTexts_.size(), false);
	}

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
}

void Console::ShowContextMenu() {
	ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 8.0f);
	ImGui::PushStyleColor(ImGuiCol_Border, { 0.25f,0.25f,0.25f,1.0f });

	// 選択された全テキストをコピーする機能（右クリックメニューから）
	if (ImGui::BeginPopupContextWindow()) {
		ImGui::Spacing();

		if (ImGui::MenuItem((StrUtils::ConvertToUtf8(kIconCopy) + " Copy Selected").c_str())) {
			std::string copiedText;
			for (size_t i = 0; i < consoleTexts_.size(); ++i) {
				if (selectedItems_[i]) {
					copiedText += consoleTexts_[i].text;
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
}

void Console::ShowAbout() {
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
		ImVec2 buttonSize = ImVec2(74.0f, 24.0f);
		ImVec2 windowSize = ImGui::GetWindowSize();  // ウィンドウ全体のサイズ
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
	[[maybe_unused]] const Vec4& color
) {
#ifdef _DEBUG
	repeatCounts_.back()++;

	const auto repeatCount = repeatCounts_.back();
	// 改行を含めない形式でメッセージを作成し、必要な場合のみ最後に追加
	auto formattedMessage = std::format("{} [x{}]{}", message, repeatCount,
		hasNewLine ? "\n" : "");

	if (repeatCount >= static_cast<int>(kConsoleRepeatError)) {
		consoleTexts_.back() = { .text = formattedMessage, .color = kConsoleColorError };
	} else if (repeatCount >= static_cast<int>(kConsoleRepeatWarning)) {
		consoleTexts_.back() = { .text = formattedMessage, .color = kConsoleColorWarning };
	} else {
		consoleTexts_.back() = { .text = formattedMessage, .color = color };
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 最下部までスクロールされている場合は自動スクロールします
//-----------------------------------------------------------------------------
void Console::CheckScroll() {
	if (bWishScrollToBottom_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
		ImGui::SetScrollHereY(1.0f);
	}

	if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
		bWishScrollToBottom_ = false;
	} else {
		bWishScrollToBottom_ = true;
	}
}

//-----------------------------------------------------------------------------
// Purpose: 行数をチェックして最大行数を超えた場合は削除します
//-----------------------------------------------------------------------------
void Console::CheckLineCount() {
#ifdef _DEBUG
	while (consoleTexts_.size() > kConsoleMaxLineCount) {
		consoleTexts_.erase(consoleTexts_.begin());
	}

	consoleTexts_.shrink_to_fit();
	history_.shrink_to_fit();
	suggestions_.shrink_to_fit();
	repeatCounts_.shrink_to_fit();
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

void Console::LogToFile(const std::string& message) {
	static std::ofstream logFile("console.log", std::ios::app);
	if (logFile.is_open()) {
		logFile << message;
	}
}

void Console::RewriteLogFile() {
	std::ofstream logFile("console.log", std::ios::trunc);
	if (logFile.is_open()) {
		for (const auto& text : consoleTexts_) {
			logFile << text.text;
		}
	}
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

size_t Console::FilteredToActualIndex(const int filteredIndex) {
	int visibleIndex = 0;
	for (size_t i = 0; i < consoleTexts_.size(); ++i) {
		if (currentFilterChannel_ == Channel::None || consoleTexts_[i].channel == currentFilterChannel_) {
			if (visibleIndex == filteredIndex) {
				return i;
			}
			visibleIndex++;
		}
	}
	return SIZE_MAX; // 該当なし
}

#ifdef _DEBUG
bool Console::bShowConsole_ = true;
bool Console::bWishScrollToBottom_ = false;
bool Console::bShowSuggestPopup_ = false;
bool Console::bShowAbout_ = false;
std::vector<Console::Text> Console::consoleTexts_;
char Console::inputText_[kInputBufferSize] = {};
int Console::historyIndex_ = -1;
std::vector<std::string> Console::history_;
std::vector<std::string> Console::suggestions_;
std::vector<uint64_t> Console::repeatCounts_;
std::vector<bool> Console::selectedItems_;
int Console::lastSelectedIndex_ = -1;
Channel Console::currentFilterChannel_ = Channel::None;
#endif
