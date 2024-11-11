#include "Console.h"

#include <cstring>
#include <format>
#include <sstream>

#include "ConVar.h"

#ifdef _DEBUG
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#endif
#include <algorithm>
#include <debugapi.h>

#include "ConVarManager.h"

#include "../Math/Vector/Vec3.h"

#include "../Utils/ClientProperties.h"

#include "../Utils/ConvertString.h"

//-----------------------------------------------------------------------------
// Purpose: コンソールの更新処理
//-----------------------------------------------------------------------------
void Console::Update() {
#ifdef _DEBUG
	if (!bShowConsole) {
		return;
	}

	ImGuiWindowFlags consoleWindowFlags = ImGuiWindowFlags_NoScrollbar; // ウィンドウにはスクロールバーを付けない

	if (bShowPopup) {
		consoleWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	ImGui::SetNextWindowSizeConstraints({ 256.0f, 256.0f }, { 8192.0f, 8192.0f });

	//ImVec2 popupPos, popupSize;

	if (ImGui::Begin("Console", &bShowConsole, consoleWindowFlags)) {
		ImVec2 size = ImGui::GetContentRegionAvail();
		size.y -= ImGui::GetFrameHeightWithSpacing() + 9.0f;

		ImGui::Spacing();

		if (ImGui::BeginChild("##scrollbox", size, true,
			ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
			for (size_t i = 0; i < consoleTexts.size(); ++i) {
				ImGui::PushStyleColor(ImGuiCol_Text, consoleTexts[i].color);
				ImGui::Selectable((consoleTexts[i].text + "##" + std::to_string(i)).c_str());
				ImGui::PopStyleColor();
			}

			// 一番下にスクロールしているか?
			if (bWishScrollToBottom && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}

			// スクロールをいじった?
			if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
				bWishScrollToBottom = false;
			} else {
				bWishScrollToBottom = true;
			}

			ScrollToBottom();
		}

		ImGui::EndChild();

		ImGui::Spacing();

		size = ImGui::GetContentRegionAvail();
		size.x -= ImGui::CalcTextSize("  Submit  ").x;

		ImGui::PushItemWidth(size.x);

		ImGuiInputTextFlags inputTextFlags =
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_EscapeClearsAll |
			ImGuiInputTextFlags_CallbackAlways |
			ImGuiInputTextFlags_CallbackCharFilter |
			ImGuiInputTextFlags_CallbackCompletion |
			ImGuiInputTextFlags_CallbackEdit |
			ImGuiInputTextFlags_CallbackHistory;

		if (ImGui::InputText("##input", inputText, IM_ARRAYSIZE(inputText), inputTextFlags, InputTextCallback)) {
			ImGui::SetKeyboardFocusHere(-1);
			SubmitCommand(inputText);
			if (!std::string(inputText).empty()) {
				history.emplace_back(inputText);
				historyIndex = static_cast<int>(history.size());
			}
			memset(inputText, 0, sizeof inputText);
		}

		//popupPos = { ImGui::GetItemRectMin().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y };
		//popupSize = { ImGui::GetItemRectSize().x, ImGui::GetTextLineHeight() * kConsoleSuggestLineCount };

		if (ImGui::IsItemActive()) {
			bShowPopup = true;
		} else {
			bShowPopup = false;
		}

		ImGui::SameLine();

		// 送信ボタン
		if (ImGui::Button(" Submit ")) {
			SubmitCommand(inputText);
			if (!std::string(inputText).empty()) {
				history.emplace_back(inputText);
				historyIndex = static_cast<int>(history.size());
			}
			memset(inputText, 0, sizeof inputText);
		}
	}
	ImGui::End();

	//static SuggestPopupState state = {
	//	false, false, -1, -1
	//};

	//static bool focused = true;

	//SuggestPopup(state, popupPos, popupSize, focused);

	if (consoleTexts.size() >= kConsoleMaxLineCount) {
		consoleTexts.erase(consoleTexts.begin());
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: リピートカウントを更新
//-----------------------------------------------------------------------------
void Console::UpdateRepeatCount([[maybe_unused]] const std::string& message, [[maybe_unused]] const ImVec4 color) {
#ifdef _DEBUG
	repeatCounts.back()++;

	if (repeatCounts.back() >= static_cast<int>(kConsoleRepeatError)) {
		consoleTexts.back() = { std::format("{} [x{}]", message, repeatCounts.back()), kConsoleColorError };
	} else if (repeatCounts.back() >= static_cast<int>(kConsoleRepeatWarning)) {
		consoleTexts.back() = { std::format("{} [x{}]", message, repeatCounts.back()), kConsoleColorWarning };
	} else {
		consoleTexts.back() = { std::format("{} [x{}]", message, repeatCounts.back()), color };
	}
#endif
}

#ifdef _DEBUG
void Console::SuggestPopup([[maybe_unused]] SuggestPopupState& state, const ImVec2& pos, const ImVec2& size,
	[[maybe_unused]] bool& isFocused) {
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
		size_t test = ConVarManager::GetInstance().GetAllConVars().size();
		if (test <= i) {
			break;
		}

		bool isIndexActive = state.activeIndex == static_cast<int>(i);
		if (isIndexActive) {
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1, 0, 0, 1));
		}

		ImGui::PushID(static_cast<int>(i));
		if (ImGui::Selectable(ConVarManager::GetInstance().GetAllConVars()[i]->GetName().c_str(), isIndexActive)) {
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

//-----------------------------------------------------------------------------
// Purpose: コンソールへ文字列を出力します
//-----------------------------------------------------------------------------
void Console::Print([[maybe_unused]] const std::string& message, [[maybe_unused]] const ImVec4 color) {
#ifdef _DEBUG
	if (message.empty()) {
		return;
	}

	if (!consoleTexts.empty() && consoleTexts.back().text.starts_with(message) && consoleTexts.back().text != "]\n") {
		// 前のメッセージと同じ場合、カウントを増加させる
		UpdateRepeatCount(message, color);
	} else {
		// 前のメッセージと異なる場合、新しいメッセージを追加
		consoleTexts.push_back({ message, color });
		repeatCounts.push_back(1);
		OutputDebugString(ConvertString::ToString(message));
	}

	bWishScrollToBottom = true;
#endif
}

//-----------------------------------------------------------------------------
// Purpose: コンソールの表示/非表示を切り替えます
//-----------------------------------------------------------------------------
void Console::ToggleConsole([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	bShowConsole = !bShowConsole;
#endif
}

#ifdef _DEBUG
//-----------------------------------------------------------------------------
// Purpose: InputTextからのイベントを処理します。 
//-----------------------------------------------------------------------------
int Console::InputTextCallback(ImGuiInputTextCallbackData* data) {
	switch (data->EventFlag) {
	case ImGuiInputTextFlags_CallbackCompletion:
		Print("Completion\n", kConsoleColorFloat);
		break;

	case ImGuiInputTextFlags_CallbackHistory:
	{
		const int prev_history_index = historyIndex;
		if (data->EventKey == ImGuiKey_UpArrow) {
			if (historyIndex > 0) {
				historyIndex--;
			}
		} else if (data->EventKey == ImGuiKey_DownArrow) {
			if (historyIndex < static_cast<int>(history.size()) - 1) {
				historyIndex++;
			} else {
				historyIndex = static_cast<int>(history.size()); // 履歴が空の場合はサイズと一致させる
			}
		}
		if (prev_history_index != historyIndex) {
			data->DeleteChars(0, data->BufTextLen);
			if (historyIndex < static_cast<int>(history.size())) {
				data->InsertChars(0, history[historyIndex].c_str());
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
// Purpose: 一番下にスクロールします
//-----------------------------------------------------------------------------
void Console::ScrollToBottom() {
#ifdef _DEBUG
	if (bWishScrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		bWishScrollToBottom = false;
	}
#endif
}

// int 型かどうかの判定
bool IsInteger(const std::string& str) {
	try {
		size_t pos;
		(void)std::stoi(str, &pos);
		// 変換後に余分な文字列がないか確認
		return pos == str.length();
	} catch ([[maybe_unused]] const std::invalid_argument& e) {
		return false;
	} catch ([[maybe_unused]] const std::out_of_range& e) {
		return false;
	}
}

// float 型かどうかの判定
bool IsFloat(const std::string& str) {
	try {
		size_t pos;
		(void)std::stof(str, &pos);
		// 変換後に余分な文字列がないか確認
		return pos == str.length();
	} catch ([[maybe_unused]] const std::invalid_argument& e) {
		return false;
	} catch ([[maybe_unused]] const std::out_of_range& e) {
		return false;
	}
}

// Vec3 かどうかの判定
bool IsVec3(const std::vector<std::string>& tokens, const int index) {
	if (index + 2 >= static_cast<int>(tokens.size())) {
		return false;
	}

	// 3つ連続するトークンがすべてfloatとして解釈できるか
	return IsFloat(tokens[index]) && IsFloat(tokens[index + 1]) && IsFloat(tokens[index + 2]);
}

// Vec3 の解析
Vec3 ParseVec3(const std::vector<std::string>& tokens, int index) {
	float x = std::stof(tokens[index]);
	float y = std::stof(tokens[index + 1]);
	float z = std::stof(tokens[index + 2]);
	return { x, y, z };
}

void Console::SubmitCommand([[maybe_unused]] const std::string& command) {
#ifdef _DEBUG
	std::string trimmedCommand = TrimSpaces(command);

	if (trimmedCommand.empty()) {
		Print("]\n", kConsoleColorNormal);
		return;
	}

	std::vector<std::string> tokens = TokenizeCommand(trimmedCommand);

	bool found = false;

	AddHistory(trimmedCommand);

	auto it = commandMap.find(tokens[0]);
	if (it != commandMap.end()) {
		it->second(tokens); // コールバックの呼び出し
		found = true;
	}

	ConVarManager& conVarManager = ConVarManager::GetInstance();

	for (auto conVar : conVarManager.GetAllConVars()) {
		// 変数が存在する場合
		if (conVar->GetName() == tokens[0]) {
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
					Print("Invalid argument...", kConsoleColorError);
				}
			}
			break;
		}
	}

	// コマンドが見つからなかった
	if (!found) {
		Print(std::format("Unknown command: {}\n", trimmedCommand));
	}

	bWishScrollToBottom = true;
#endif
}

void Console::RegisterCommand([[maybe_unused]] const std::string& commandName,
	[[maybe_unused]] const CommandCallback& callback) {
#ifdef _DEBUG
	commandMap[commandName] = callback;
#endif
}

void Console::Clear([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	consoleTexts.clear();
#endif
}

void Console::Help([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	for (auto conVar : ConVarManager::GetInstance().GetAllConVars()) {
		Print(" - " + conVar->GetName() + " : " + conVar->GetHelp() + "\n");
	}
#endif
}

void Console::AddHistory([[maybe_unused]] const std::string& command) {
#ifdef _DEBUG
	consoleTexts.push_back({ "] " + command, ImVec4(0.8f, 1.0f, 1.0f, 1.0f) });
#endif
}

std::string Console::TrimSpaces(const std::string& string) {
	const size_t start = string.find_first_not_of(" \t\n\r");
	const size_t end = string.find_last_not_of(" \t\n\r");

	if (start == std::string::npos || end == std::string::npos) {
		return "";
	}

	return string.substr(start, end - start + 1);
}

std::vector<std::string> Console::TokenizeCommand(const std::string& command) {
	std::istringstream stream(command);
	std::vector<std::string> tokens;
	std::string token;

	while (stream >> token) {
		tokens.push_back(token);
	}

	return tokens;
}
