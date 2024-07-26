#include "Console.h"

#include <format>
#include <sstream>

#include "ConVar.h"
#include "ConVars.h"
#include "imgui/imgui_internal.h"
#include "Source/Engine/Utils/ConvertString.h"

#ifdef _DEBUG

#include "imgui/imgui.h"
#include "Source/Engine/Utils/ClientProperties.h"
#include <cstring>

int ConsoleCallback(ImGuiInputTextCallbackData* data) {
	switch (data->EventFlag) {
	case ImGuiInputTextFlags_CallbackCompletion:
		Console::Print("Completion", kConsoleWarning);
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
				historyIndex = static_cast<int>(history.size());  // 履歴が空の場合はサイズと一致させる
			}
		}
		if (prev_history_index != historyIndex) {
			data->DeleteChars(0, data->BufTextLen);
			if (historyIndex < static_cast<int>(history.size())) {
				data->InsertChars(0, history[historyIndex].c_str());
			} else {
				data->InsertChars(0, "");  // 履歴が空の場合は空文字列を挿入
			}
		}
	}
	break;

	case ImGuiInputTextFlags_CallbackEdit:
		break;

	case ImGuiInputTextFlags_CallbackResize:
		Console::Print("Resize	", kConsoleError);
		break;
	default:;
	}
	return 0;
}

void Console::Init() {
}

void Console::Update() {
	if (!bShowConsole) {
		return;
	}

	ImGuiWindowFlags consoleWindowFlags =
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoBringToFrontOnFocus |
		ImGuiWindowFlags_NoDocking;

	if (bShowPopup) {
		consoleWindowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
	}

	ImGui::SetNextWindowSizeConstraints({ 256.0f,256.0f }, { 8192.0f,8192.0f });

	bool open = ImGui::Begin("Console", &bShowConsole, consoleWindowFlags);

	if (open) {
		ImVec2 size = ImGui::GetContentRegionAvail();
		size.y -= ImGui::GetFrameHeightWithSpacing() + 9.0f;

		ImGui::Spacing();

		if (ImGui::BeginChild("##scrollbox", size, true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
			for (const ConsoleText& consoleText : consoleTexts) {
				ImGui::PushStyleColor(ImGuiCol_Text, consoleText.color);
				ImGui::Selectable((consoleText.text).c_str());
				ImGui::PopStyleColor();
			}

			// 一番下にスクロールしているか?
			if (wishScrollToBottom && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
				ImGui::SetScrollHereY(1.0f);
			}

			// スクロールをいじった?
			if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
				wishScrollToBottom = false;
			} else {
				wishScrollToBottom = true;
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

		if (ImGui::InputText("##input", str, IM_ARRAYSIZE(str), inputTextFlags, reinterpret_cast<ImGuiInputTextCallback>(ConsoleCallback))) {
			ImGui::SetKeyboardFocusHere(-1);
			SubmitCommand(str);
			if (!std::string(str).empty()) {
				history.emplace_back(str);
				historyIndex = static_cast<int>(history.size());
			}
			memset(str, 0, sizeof str);
		}

		if (ImGui::IsItemActive()) {
			bShowPopup = true;
		} else {
			bShowPopup = false;
		}

		ImGui::SameLine();

		if (ImGui::Button(" Submit ")) {
			SubmitCommand(str);
			if (!std::string(str).empty()) {
				history.emplace_back(str);
				historyIndex = static_cast<int>(history.size());
			}
			memset(str, 0, sizeof str);
		}
	}
	ImGui::End();

	if (consoleTexts.size() >= kConsoleMaxLineCount) {
		consoleTexts.erase(consoleTexts.begin());
	}
}

void Console::UpdateRepeatCount(const std::string& message, const ImVec4 color) {
	repeatCounts.back()++;

	if (repeatCounts.back() >= kConsoleRepeatError) {
		consoleTexts.back() = { std::format("{} [x{}]", message, repeatCounts.back()), kConsoleError };
	} else if (repeatCounts.back() >= kConsoleRepeatWarning) {
		consoleTexts.back() = { std::format("{} [x{}]", message, repeatCounts.back()), kConsoleWarning };
	} else {
		consoleTexts.back() = { std::format("{} [x{}]", message, repeatCounts.back()), color };
	}
}

void Console::Print(const std::string& message, const ImVec4 color) {
	if (message.empty()) {
		return;
	}

	if (!consoleTexts.empty() && consoleTexts.back().text.find(message) == 0) {
		// 前のメッセージと同じ場合、カウントを増加させる
		UpdateRepeatCount(message, color);
	} else {
		// 前のメッセージと異なる場合、新しいメッセージを追加
		consoleTexts.push_back({ message, color });
		repeatCounts.push_back(1);
		OutputDebugString(ConvertString(message + "\n").c_str());
	}

	wishScrollToBottom = true;
}

// ログファイルにに出力
void Console::OutputLog(std::string filepath, std::string log) {
	filepath;
	log;
}

void Console::ToggleConsole() {
	bShowConsole = !bShowConsole;
}

void Console::ScrollToBottom() {
	if (wishScrollToBottom) {
		ImGui::SetScrollHereY(1.0f);
		wishScrollToBottom = false;
	}
}

void Console::SubmitCommand(const std::string& command) {
	std::string trimmedCommand = TrimSpaces(command);

	if (trimmedCommand.empty()) {
		return;
	}

	const std::vector<std::string> tokens = TokenizeCommand(trimmedCommand);

	AddHistory(trimmedCommand);

	if (ConVars::GetInstance().GetAllConVars().contains(tokens[0])) {
		if (tokens.size() < 2) {
			Print(ConVars::GetInstance().GetConVar(tokens[0])->GetDescription());
		}

		for (int i = 1; i < tokens.size(); ++i) {
			ConVars::GetInstance().GetConVar(tokens[0])->SetValue(std::stoi(tokens[i]));
		}
	} else {
		Print(std::format("Unknown command: {}", trimmedCommand));
	}

	repeatCounts.push_back(1);
	OutputDebugString(ConvertString(command + "\n").c_str());

	wishScrollToBottom = true;
}

void Console::AddHistory(const std::string& command) {
	consoleTexts.push_back({ "> " + command,ImVec4(0.8f,1.0f,1.0f,1.0f) });
}

void Console::ShowPopup() {
	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 windowSize = ImGui::GetWindowSize();
	ImVec2 inputTextRectMin = ImGui::GetItemRectMin();
	ImVec2 inputTextRectMax = ImGui::GetItemRectMax();

	ImGuiWindowFlags popupFlags =
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_HorizontalScrollbar |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus;

	// 見た目が悪いので角を丸めない
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	// テキストボックスの上に配置
	ImGui::SetNextWindowPos(ImVec2(inputTextRectMin.x, windowPos.y + windowSize.y));
	// テキストボックスの幅に設定
	ImGui::SetNextWindowSize(ImVec2(inputTextRectMax.x - inputTextRectMin.x, ImGui::GetTextLineHeightWithSpacing() * kConsoleSuggestLineCount));

	if (ImGui::Begin("history_popup", nullptr, popupFlags)) {
		ImGui::PushAllowKeyboardFocus(false);

		for (auto h : history) {
			ImGui::Selectable(h.c_str());
		}

		ImGui::SetScrollHereY(1.0f);

		ImGui::PopAllowKeyboardFocus();
	}

	ImGui::End();

	ImGui::PopStyleVar();
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

#endif
