#include "Console.h"

#include "ConVar.h"
#include "ConVars.h"
#include "imgui/imgui_internal.h"
#include "Source/Engine/Utils/ConvertString.h"

#ifdef _DEBUG

#include "imgui/imgui.h"
#include "Source/Engine/Utils/ClientProperties.h"
#include <format>

struct InputTextCallback {
	bool isPopupOpen;
	int activeIndex;		// Index of currently 'activate' item by use of up/down keys
	int clickedIndex;		// Index of popup item clicked with the mouse
	bool selectionChanged;	// Flag to help focus the correct item when selecting active item
};

void Console::Init() {
}

void Console::Update() {
	if (!bShowConsole) {
		return;
	}

	InputTextCallback callback = {
		false,
		-1,
		-1,
		false
	};

	if (ImGui::Begin("Console", &bShowConsole, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus)) {
		// バツボタンの位置とサイズを設定
		ImVec2 closeButtonPos = ImGui::GetWindowPos();
		closeButtonPos.x += ImGui::GetWindowWidth() - 20; // ウィンドウ右上に配置
		closeButtonPos.y += 5; // 少し下に調整

		ImGui::SetCursorScreenPos(closeButtonPos);

		// バツボタンの描画
		if (ImGui::SmallButton("X")) {
			bShowConsole = false; // ボタンがクリックされたらウィンドウを閉じる
		}

		ImVec2 size = ImGui::GetContentRegionAvail();
		size.y -= ImGui::GetFrameHeightWithSpacing() + 9.0f;

		ImGui::Spacing();

		ImGui::BeginChild("##scrollbox", size, true, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);

		for (const ConsoleText& consoleText : history_) {
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

		ImGui::Spacing();

		size = ImGui::GetContentRegionAvail();
		size.x -= ImGui::CalcTextSize("  Submit  ").x;

		ImGui::PushItemWidth(size.x);

		ImGuiInputTextFlags flags =
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackAlways |
			ImGuiInputTextFlags_CallbackCharFilter |
			ImGuiInputTextFlags_CallbackCompletion |
			ImGuiInputTextFlags_CallbackHistory;

		flags;

		const size_t INPUT_BUF_SIZE = 512;
		static char str[INPUT_BUF_SIZE] = { 0 };

		ImGuiInputTextFlags inputTextFlags =
			ImGuiInputTextFlags_EnterReturnsTrue |
			ImGuiInputTextFlags_CallbackAlways |
			ImGuiInputTextFlags_CallbackCharFilter |
			ImGuiInputTextFlags_CallbackCompletion |
			ImGuiInputTextFlags_CallbackHistory;




		if (ImGui::InputText("##input", str, IM_ARRAYSIZE(str), inputTextFlags, callback, &InputCallback)) {
			ImGui::SetKeyboardFocusHere(-1);


			SubmitCommand(str);
			memset(str, 0, sizeof str);
		}

		ImGui::SameLine();
		if (ImGui::Button(" Submit ")) {
			SubmitCommand(str);
			memset(str, 0, sizeof str);
		}

		ImGui::End();
	}

	if (history_.size() >= kConsoleMaxLineCount) {
		history_.erase(history_.begin());
	}
}

void Console::Print(const std::string& message, const ImVec4 color) {
	if (message.empty()) {
		return;

	}

	if (!history_.empty() && history_.back().text.find(message) == 0) {
		// 前のメッセージと同じ場合、カウントを増加させる
		repeatCounts_.back()++;
		history_.back() = { std::format("{} [x{}]", message, repeatCounts_.back()), color };
	} else {
		// 前のメッセージと異なる場合、新しいメッセージを追加
		history_.push_back({ message, color });
		repeatCounts_.push_back(1);
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
	if (command.empty()) {
		return;
	}

	if (!history_.empty() && history_.back().text.find("] " + command) == 0) {
		// 前のメッセージと同じ場合はカウントを増加させる
		repeatCounts_.back()++;
		history_.back() = { std::format("] {} [x{}]", command, repeatCounts_.back()), ImVec4(1.0f,1.0f,1.0f,1.0f) };
	} else {
		// 前のメッセージと違う場合は新しいメッセージを追加
		history_.push_back({ "] " + command,ImVec4(1.0f,1.0f,1.0f,1.0f) });
		repeatCounts_.push_back(1);
		OutputDebugString(ConvertString(command + "\n").c_str());
	}

	wishScrollToBottom = true;
}

#endif