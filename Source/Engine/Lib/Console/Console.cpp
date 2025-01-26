#include "Console.h"

#include <format>

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

	ImGuiWindowFlags consoleWindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse;

	if (bShowPopup_) {
		consoleWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	ImGui::SetNextWindowSizeConstraints({ 360.0f, 360.0f }, { 8192.0f, 8192.0f });

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));
	bool bWindowOpen = ImGui::Begin("コンソール", &bShowConsole_, consoleWindowFlags);
	ImGui::PopStyleVar();

	if (bWindowOpen) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 3.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 16.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);

		// 入力フィールドとボタンの高さを取得
		float inputTextHeight = ImGui::GetFrameHeightWithSpacing();

		// 子ウィンドウの高さを調整
		auto child_size = ImVec2(0, -inputTextHeight - ImGui::GetStyle().ItemSpacing.y);
		if (ImGui::BeginChild(
			"##scrollbox", child_size, true,
			ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar |
			ImGuiWindowFlags_NoDecoration
		)) {
			for (size_t i = 0; i < consoleTexts_.size(); ++i) {
				ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(consoleTexts_[i].color));
				ImGui::Selectable((consoleTexts_[i].text + "##" + std::to_string(i)).c_str());
				ImGui::PopStyleColor();
			}

			if (bWishScrollToBottom_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}

			if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
				bWishScrollToBottom_ = false;
			} else {
				bWishScrollToBottom_ = true;
			}

			ScrollToBottom();
		}

		ImGui::EndChild();

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
				Print(
					std::format(
						R"("{}" = "{}")",
						conVar->GetName(),
						conVar->GetValueAsString()
					),
					kConsoleColorWarning
				);
				// 変数の説明を取得
				std::string description = conVar->GetHelp();
				// 変数の型を取得
				std::string type = std::format("[{}]\n", conVar->GetTypeAsString());

				if (conVar->GetTypeAsString() == "bool") {
					Print(" - " + description + " " + type, kConsoleColorBool);
				} else if (conVar->GetTypeAsString() == "int") {
					Print(" - " + description + " " + type, kConsoleColorInt);
				} else if (conVar->GetTypeAsString() == "float") {
					Print(" - " + description + " " + type, kConsoleColorFloat);
				} else if (conVar->GetTypeAsString() == "Vec3") {
					Print(" - " + description + " " + type, kConsoleColorVec3);
				} else if (conVar->GetTypeAsString() == "string") {
					Print(" - " + description + " " + type, kConsoleColorString);
				}
			} else {
				// 引数込みで入力された場合の処理
				bool isValidInput = true;
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
						if (tokens[i] != "true" && tokens[i] != "false") {
							isValidInput = false;
							break;
						}
					}
					// Vec3やstringは追加のチェックが必要な場合に応じて処理する
				}
				if (isValidInput) {
					for (size_t i = 1; i < tokens.size(); ++i) {
						conVar->SetValueFromString(tokens[i]);
					}
				} else {
					Print("CVARの型を変換できませんでした", kConsoleColorError, Channel::kConsole);
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
// Purpose: コンソールへ文字列を出力します
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

	if (channel != Channel::kNone) {
		msg = "[ " + ToString(channel) + " ] " + message;
	}

	if (!consoleTexts_.empty() && consoleTexts_.back().text.starts_with(msg) && consoleTexts_.back().text != "]\n") {
		// 前のメッセージと同じ場合、カウントを増加させる
		UpdateRepeatCount(msg, color);
	} else {
		// 前のメッセージと異なる場合、新しいメッセージを追加
		consoleTexts_.push_back({ msg, color });
		repeatCounts_.push_back(1);
		OutputDebugString(StrUtils::ToString(msg));
	}

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
// Purpose: リピートカウントを更新
//-----------------------------------------------------------------------------
void Console::UpdateRepeatCount([[maybe_unused]] const std::string& message, [[maybe_unused]] const Vec4 color) {
#ifdef _DEBUG
	repeatCounts_.back()++;

	const auto repeatCount = repeatCounts_.back();
	const auto formattedMessage = std::format("{} [x{}]", message, repeatCount);

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
		Print(" - " + conVar->GetName() + " : " + conVar->GetHelp() + "\n", kConsoleColorNormal, Channel::kNone);
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

	const std::vector info = {
		prompt + "\n",
		(!prompt.empty() ? std::string(prompt.size(), '-') : "") + "\n",
		uptime + "\n",
		"Resolution:  " + std::to_string(Window::GetClientWidth()) + "x" + std::to_string(Window::GetClientHeight()) +
		"\n",
		"CPU:  " + WindowsUtils::GetCPUName() + "\n",
		"GPU:  " + WindowsUtils::GetGPUName() + "\n",
		"Memory:  " + WindowsUtils::GetRamUsage() + " / " + WindowsUtils::GetRamMax() + "\n"
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
		Print(line, kConsoleColorWait, Channel::kNone);
	}
}

//-----------------------------------------------------------------------------
// Purpose: 入力された文字列をエコーします
//-----------------------------------------------------------------------------
void Console::Echo(const std::vector<std::string>& args) {
	Print(StrUtils::Join(args, " ") + "\n", kConsoleColorNormal, Channel::kConsole);
}

//-----------------------------------------------------------------------------
// Purpose: チャンネルを文字列に変換します
//-----------------------------------------------------------------------------
std::string Console::ToString(const Channel& e) {
	switch (e) {
	case Channel::kNone: return "";
	case Channel::kConsole: return "Console";
	case Channel::kEngine: return "Engine";
	case Channel::kGeneral: return "General";
	case Channel::kDeveloper: return "Developer";
	case Channel::kClient: return "Client";
	case Channel::kServer: return "Server";
	case Channel::kHost: return "Host";
	case Channel::kGame: return "Game";
	case Channel::kInputSystem: return "InputSystem";
	case Channel::kSound: return "Sound";
	case Channel::kPhysics: return "Physics";
	case Channel::kRenderPipeline: return "RenderPipeline";
	case Channel::kRenderSystem: return "RenderSystem";
	case Channel::kUserInterface: return "UserInterface";
	default: return "unknown";
	}
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
	case ImGuiInputTextFlags_CallbackCompletion: Print("Completion\n", kConsoleColorFloat);
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
// Purpose: 一番下にスクロールします
//-----------------------------------------------------------------------------
void Console::ScrollToBottom() {
#ifdef _DEBUG
	if (bWishScrollToBottom_) {
		ImGui::SetScrollHereY(1.0f);
		bWishScrollToBottom_ = false;
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: コマンド履歴に追加します
//-----------------------------------------------------------------------------
void Console::AddCommandHistory([[maybe_unused]] const std::string& command) {
#ifdef _DEBUG
	consoleTexts_.push_back({ .text = "> " + command, .color = {.x = 0.8f, .y = 1.0f, .z = 1.0f, .w = 1.0f} });
#endif
}

//-----------------------------------------------------------------------------
// Purpose: コンソールの行数をチェックします
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

#ifdef _DEBUG
bool Console::bShowConsole_ = false;
bool Console::bWishScrollToBottom_ = false;
bool Console::bShowPopup_ = false;
std::vector<Console::Text> Console::consoleTexts_;
char Console::inputText_[kInputBufferSize] = {};
int Console::historyIndex_ = -1;
std::vector<std::string> Console::history_;
std::vector<std::string> Console::suggestions_;
std::vector<uint64_t> Console::repeatCounts_;
#endif
