#include "Console.h"

#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"

void Console::Init() {
}

void Console::Update() {
	if (!bShowConsole) {
		return;
	}

	static char str[256];

	if (ImGui::Begin("Console", 0, ImGuiWindowFlags_NoScrollbar)) {
		//ImGui::ListBox("##listbox",)
		ImVec2 size = ImGui::GetContentRegionAvail();
		size.y -= ImGui::GetFrameHeightWithSpacing() + 6.0f;

		ImGui::BeginChild("##scrollbox", size, true, ImGuiWindowFlags_HorizontalScrollbar & ImGuiWindowFlags_AlwaysVerticalScrollbar);

		for (int i = 0; i < history_.size(); ++i) {
			ImGui::Selectable(("] " + history_[i]).c_str());

			if (wishScrollToBottom && history_.size() < i) {
				ImGui::SetScrollHereY(0.5f);
				wishScrollToBottom = false;
			}
		}

		for (auto command : history_) {

		}

		ImGui::EndChild();

		ImGui::Spacing();

		size = ImGui::GetContentRegionAvail();
		size.x -= ImGui::CalcTextSize("  Submit  ").x;

		ImGui::PushItemWidth(size.x);
		if (ImGui::InputText("##input", str, IM_ARRAYSIZE(str), ImGuiInputTextFlags_EnterReturnsTrue)) {
			ImGui::SetKeyboardFocusHere(-1);
			SubmitCommand(str);
		}
		ImGui::SameLine();
		if (ImGui::Button(" Submit ")) {
			SubmitCommand(str);
		}
	}
	ImGui::End();
}

void Console::Print(std::string message) {
	message;
}

void Console::OutputLog(std::string filepath, std::string log) {
	filepath;
	log;
}

void Console::ToggleConsole() {
	bShowConsole = !bShowConsole;
}

void Console::SubmitCommand(const std::string& command) {
	history_.push_back(command);
	wishScrollToBottom = true;
}