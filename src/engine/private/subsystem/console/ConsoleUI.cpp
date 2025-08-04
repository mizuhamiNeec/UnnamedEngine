#ifdef _DEBUG
#include <string>

#include <engine/public/subsystem/console/ConsoleUI.h>
#include <engine/public/subsystem/console/Log.h>

#include <imgui.h>

namespace Unnamed {
	ConsoleUI::ConsoleUI(
		ConsoleSystem* consoleSystem
	): mConsoleSystem(consoleSystem) {
	}

	/// @brief コンソールUIを表示します。
	/// @details ImGuiのコンテキスト内で呼び出してください。
	void ConsoleUI::Show() {
		ImGui::Begin("Console##ConsoleSystem");
		for (auto buffer : mConsoleSystem->GetLogBuffer()) {
			PushLogTextColor(buffer);

			std::string text = "[ " + buffer.channel + " ] " + buffer.
				message;

			ImGui::Text(text.c_str());

			ImGui::PopStyleColor();
		}
		ImGui::End();
	}

	/// @brief コンソールログのテキストの色を設定します。
	/// @param buffer コンソールログのテキスト情報
	void ConsoleUI::PushLogTextColor(const ConsoleLogText& buffer) {
		switch (buffer.level) {
		case LogLevel::None:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColor));
			break;
		case LogLevel::Info:
			ImGui::PushStyleColor(ImGuiCol_Text, ToImVec4(kConTextColor));
			break;
		case LogLevel::Dev:
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      ToImVec4(kConTextColorDev));
			break;
		case LogLevel::Warning:
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      ToImVec4(kConTextColorWarn));
			break;
		case LogLevel::Error:
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      ToImVec4(kConTextColorError));
			break;
		case LogLevel::Fatal:
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      ToImVec4(kConTextColorFatal));
			break;
		case LogLevel::Execute:
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      ToImVec4(kConTextColorExec));
			break;
		case LogLevel::Waiting:
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      ToImVec4(kConTextColorWait));
			break;
		case LogLevel::Success:
			ImGui::PushStyleColor(ImGuiCol_Text,
			                      ToImVec4(kConTextColorSuccess));
			break;
		}
	}
}
#endif