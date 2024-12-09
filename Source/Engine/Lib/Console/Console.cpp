#include "Console.h"

#include <cstring>
#include <format>
#include <sstream>

#include "ConVar.h"

#include <debugapi.h>

#include "ConCommand.h"
#include "ConVarManager.h"
#include "../Utils/ClientProperties.h"
#include "../Utils/StrUtils.h"

//-----------------------------------------------------------------------------
// Purpose: コンソールの更新処理
//-----------------------------------------------------------------------------
void Console::Update() {
#ifdef _DEBUG
	if (!bShowConsole) {
		return;
	}

	ImGuiWindowFlags consoleWindowFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoCollapse;

	if (bShowPopup) {
		consoleWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	ImGui::SetNextWindowSizeConstraints({ 360.0f, 360.0f }, { 8192.0f, 8192.0f });

	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.0f, 10.0f));
	bool bWindowOpen = ImGui::Begin("Console", &bShowConsole, consoleWindowFlags);
	ImGui::PopStyleVar();

	if (bWindowOpen) {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.0f, 3.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 16.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 0.0f);

		// 入力フィールドとボタンの高さを取得
		float inputTextHeight = ImGui::GetFrameHeightWithSpacing();

		// 子ウィンドウの高さを調整
		ImVec2 child_size = ImVec2(0, -inputTextHeight - ImGui::GetStyle().ItemSpacing.y);
		if (ImGui::BeginChild(
			"##scrollbox", child_size, true,
			ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar |
			ImGuiWindowFlags_NoDecoration
		)) {
			for (size_t i = 0; i < consoleTexts.size(); ++i) {
				ImGui::PushStyleColor(ImGuiCol_Text, consoleTexts[i].color);
				ImGui::Selectable((consoleTexts[i].text + "##" + std::to_string(i)).c_str());
				ImGui::PopStyleColor();
			}

			if (bWishScrollToBottom && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}

			if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
				bWishScrollToBottom = false;
			} else {
				bWishScrollToBottom = true;
			}

			ScrollToBottom();
		}

		ImGui::EndChild();

		ImGui::Spacing();

		ImVec2 size = ImGui::GetContentRegionAvail();
		float buttonWidth = ImGui::CalcTextSize(" Submit ").x + ImGui::GetStyle().FramePadding.x * 2.0f;
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

		if (ImGui::InputText("##input", inputText, IM_ARRAYSIZE(inputText), inputTextFlags, InputTextCallback)) {
			ImGui::SetKeyboardFocusHere(-1);
			SubmitCommand(inputText);
			if (!std::string(inputText).empty()) {
				history.emplace_back(inputText);
				historyIndex = static_cast<int>(history.size());
			}
			memset(inputText, 0, sizeof inputText);
		}

		ImGui::SameLine();

		if (ImGui::Button(" Submit ")) {
			SubmitCommand(inputText);
			if (!std::string(inputText).empty()) {
				history.emplace_back(inputText);
				historyIndex = static_cast<int>(history.size());
			}
			memset(inputText, 0, sizeof inputText);
		}

		ImGui::PopStyleVar(3);
	}
	ImGui::End();

	if (consoleTexts.size() >= kConsoleMaxLineCount) {
		consoleTexts.erase(consoleTexts.begin());
		consoleTexts.shrink_to_fit();
		history.shrink_to_fit();
		suggestions.shrink_to_fit();
		repeatCounts.shrink_to_fit();
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: リピートカウントを更新
//-----------------------------------------------------------------------------
void Console::UpdateRepeatCount([[maybe_unused]] const std::string& message, [[maybe_unused]] const ImVec4 color) {
#ifdef _DEBUG
	repeatCounts.back()++;

	const auto repeatCount = repeatCounts.back();
	const auto formattedMessage = std::format("{} [x{}]", message, repeatCount);

	if (repeatCount >= static_cast<int>(kConsoleRepeatError)) {
		consoleTexts.back() = { formattedMessage, kConsoleColorError };
	} else if (repeatCount >= static_cast<int>(kConsoleRepeatWarning)) {
		consoleTexts.back() = { formattedMessage, kConsoleColorWarning };
	} else {
		consoleTexts.back() = { formattedMessage, color };
	}
#endif
}

#ifdef _DEBUG
void Console::SuggestPopup(
	[[maybe_unused]] SuggestPopupState& state,
	const ImVec2& pos,
	const ImVec2& size,
	[[maybe_unused]] bool& isFocused
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
		OutputDebugString(StrUtils::ToString(message));
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

float Console::scrollAnimationSpeed = 0.1f;

void Console::SubmitCommand([[maybe_unused]] const std::string& command) {
#ifdef _DEBUG
	std::string trimmedCommand = TrimSpaces(command);

	// コマンドが空なのでなんもしない
	if (trimmedCommand.empty()) {
		Print("]\n", kConsoleColorNormal);
		return;
	}

	std::vector<std::string> tokens = TokenizeCommand(trimmedCommand);

	bool found = false;

	// とりあえず履歴に追加
	AddHistory(trimmedCommand);

	found = ConCommand::ExecuteCommand(trimmedCommand);

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
					Print("what ?", kConsoleColorError);
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

void Console::Clear([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	consoleTexts.clear(); // コンソールのテキストをクリア
	consoleTexts.shrink_to_fit(); // 開放
	history.clear(); // コマンド履歴をクリア
	history.shrink_to_fit();
	suggestions.clear(); // サジェストをクリア
	suggestions.shrink_to_fit();
	repeatCounts.clear(); // 繰り返しカウントをクリア
	repeatCounts.shrink_to_fit();
	historyIndex = -1; // 履歴インデックスを初期化
	bWishScrollToBottom = true; // 再描画の際にスクロールをリセット
#endif
}

void Console::Help([[maybe_unused]] const std::vector<std::string>& args) {
#ifdef _DEBUG
	ConCommand::Help();
	for (auto conVar : ConVarManager::GetAllConVars()) {
		Print(" - " + conVar->GetName() + " : " + conVar->GetHelp() + "\n");
	}
#endif
}

void Console::Neofetch([[maybe_unused]] const std::vector<std::string>& args) {
	char* osEnv = nullptr;
	size_t osEnvSize = 0;
	_dupenv_s(&osEnv, &osEnvSize, "OS");
	if (osEnv) {
		Print("OS : " + std::string(osEnv) + "\n", kConsoleColorNormal);
		free(osEnv);
	} else {
		Print("OS : Not found\n", kConsoleColorNormal);
	}

	char* usernameEnv = nullptr;
	size_t usernameEnvSize = 0;
	_dupenv_s(&usernameEnv, &usernameEnvSize, "USERNAME");
	if (usernameEnv) {
		Print("Username : " + std::string(usernameEnv) + "\n", kConsoleColorNormal);
		free(usernameEnv);
	} else {
		Print("Username : Not found\n", kConsoleColorNormal);
	}

	char* computerNameEnv = nullptr;
	size_t computerNameEnvSize = 0;
	_dupenv_s(&computerNameEnv, &computerNameEnvSize, "COMPUTERNAME");
	if (computerNameEnv) {
		Print("Computer Name : " + std::string(computerNameEnv) + "\n", kConsoleColorNormal);
		free(computerNameEnv);
	} else {
		Print("Computer Name : Not found\n", kConsoleColorNormal);
	}

	char* processorEnv = nullptr;
	size_t processorEnvSize = 0;
	_dupenv_s(&processorEnv, &processorEnvSize, "PROCESSOR_IDENTIFIER");
	if (processorEnv) {
		Print("Processor : " + std::string(processorEnv) + "\n", kConsoleColorNormal);
		free(processorEnv);
	} else {
		Print("Processor : Not found\n", kConsoleColorNormal);
	}

	char* numProcessorsEnv = nullptr;
	size_t numProcessorsEnvSize = 0;
	_dupenv_s(&numProcessorsEnv, &numProcessorsEnvSize, "NUMBER_OF_PROCESSORS");
	if (numProcessorsEnv) {
		Print("Number of Processors : " + std::string(numProcessorsEnv) + "\n", kConsoleColorNormal);
		free(numProcessorsEnv);
	} else {
		Print("Number of Processors : Not found\n", kConsoleColorNormal);
	}

	char* systemDriveEnv = nullptr;
	size_t systemDriveEnvSize = 0;
	_dupenv_s(&systemDriveEnv, &systemDriveEnvSize, "SystemDrive");
	if (systemDriveEnv) {
		Print("System Drive : " + std::string(systemDriveEnv) + "\n", kConsoleColorNormal);
		free(systemDriveEnv);
	} else {
		Print("System Drive : Not found\n", kConsoleColorNormal);
	}

	char* systemRootEnv = nullptr;
	size_t systemRootEnvSize = 0;
	_dupenv_s(&systemRootEnv, &systemRootEnvSize, "SystemRoot");
	if (systemRootEnv) {
		Print("System Root : " + std::string(systemRootEnv) + "\n", kConsoleColorNormal);
		free(systemRootEnv);
	} else {
		Print("System Root : Not found\n", kConsoleColorNormal);
	}

	char* userProfileEnv = nullptr;
	size_t userProfileEnvSize = 0;
	_dupenv_s(&userProfileEnv, &userProfileEnvSize, "USERPROFILE");
	if (userProfileEnv) {
		Print("User Profile : " + std::string(userProfileEnv) + "\n", kConsoleColorNormal);
		free(userProfileEnv);
	} else {
		Print("User Profile : Not found\n", kConsoleColorNormal);
	}
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

#ifdef _DEBUG
bool Console::bShowConsole = true;
bool Console::bWishScrollToBottom = false;
bool Console::bShowPopup = false;
std::vector<ConsoleText> Console::consoleTexts;
char Console::inputText[kInputBufferSize] = {};
int Console::historyIndex = -1;
std::vector<std::string> Console::history;
std::vector<std::string> Console::suggestions;
std::vector<int> Console::repeatCounts;
#endif
