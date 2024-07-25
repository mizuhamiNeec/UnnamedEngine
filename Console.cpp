#include "Console.h"

#include <sstream>

#include "ConVar.h"
#include "ConVars.h"
#include "imgui/imgui_internal.h"
#include "Source/Engine/Utils/ConvertString.h"

#ifdef _DEBUG

#include "imgui/imgui.h"
#include "Source/Engine/Utils/ClientProperties.h"
#include <format>

int ConsoleCallback(const ImGuiInputTextCallbackData* data) {
	switch (data->EventFlag) {
	case ImGuiInputTextFlags_CallbackCompletion:
		Console::Print("Completion", kConsoleWarning);
		break;

	case ImGuiInputTextFlags_CallbackHistory:
		Console::Print("History", kConsoleError);
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

	bool open = ImGui::Begin("Console", &bShowConsole, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);

	// 閉じるボタンを表示
	ImVec2 windowSize = ImGui::GetWindowSize();
	ImVec2 closeButtonPos = ImVec2(windowSize.x - 20, 5);
	if (ImGui::CloseButton(ImGui::GetID("##closeButton"), closeButtonPos)) {
		bShowConsole = false; // ボタンがクリックされたらウィンドウを閉じる
	}

	if (open) {
		ImVec2 size = ImGui::GetContentRegionAvail();
		size.y -= ImGui::GetFrameHeightWithSpacing() + 9.0f;

		ImGui::Spacing();

		if (ImGui::BeginChild("##scrollbox", size, true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
			for (const ConsoleText& consoleText : history) {
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

			ImGui::EndChild();
		}

		ImGui::Spacing();

		size = ImGui::GetContentRegionAvail();
		size.x -= ImGui::CalcTextSize("  Submit  ").x;

		ImGui::PushItemWidth(size.x);

		static char str[kInputBufferSize] = {0};

		ImGuiInputTextFlags inputTextFlags =
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackAlways |
			ImGuiInputTextFlags_CallbackCharFilter |
			ImGuiInputTextFlags_CallbackCompletion |
			ImGuiInputTextFlags_CallbackHistory;

		if (ImGui::InputText("##input", str, IM_ARRAYSIZE(str), inputTextFlags, reinterpret_cast<ImGuiInputTextCallback>(ConsoleCallback))) {
			ImGui::SetKeyboardFocusHere(-1);

			SubmitCommand(str);
			memset(str, 0, sizeof str);
		}

		ImGui::SameLine();

		if (ImGui::Button(" Submit ")) {
			SubmitCommand(str);
			memset(str, 0, sizeof str);
		}
	}
	ImGui::End();

	if (history.size() >= kConsoleMaxLineCount) {
		history.erase(history.begin());
	}
}

void Console::UpdateRepeatCount(const std::string& message, const ImVec4 color) {
	repeatCounts.back()++;

	if (repeatCounts.back() >= kConsoleRepeatError) {
		history.back() = {std::format("{} [x{}]", message, repeatCounts.back()), kConsoleError};
	} else if (repeatCounts.back() >= kConsoleRepeatWarning) {
		history.back() = {std::format("{} [x{}]", message, repeatCounts.back()), kConsoleWarning};
	} else {
		history.back() = {std::format("{} [x{}]", message, repeatCounts.back()), color};
	}
}

void Console::Print(const std::string& message, const ImVec4 color) {
	if (message.empty()) {
		return;
	}

	if (!history.empty() && history.back().text.find(message) == 0) {
		// 前のメッセージと同じ場合、カウントを増加させる
		UpdateRepeatCount(message, color);
	} else {
		// 前のメッセージと異なる場合、新しいメッセージを追加
		history.push_back({message, color});
		repeatCounts.push_back(1);
		OutputDebugString(ConvertString(message + "\n").c_str());
	}

	wishScrollToBottom = true;
}

// ログに出力
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
		ConVars::GetInstance().GetConVar(tokens[0])->SetValue(std::stoi(tokens[1]));
	} else {
		Print(std::format("Unknown command: {}", trimmedCommand));
	}

	repeatCounts.push_back(1);
	OutputDebugString(ConvertString(command + "\n").c_str());


	wishScrollToBottom = true;
}

void Console::AddHistory(const std::string& command) {
	history.push_back({"> " + command,ImVec4(0.8f,1.0f,1.0f,1.0f)});
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
