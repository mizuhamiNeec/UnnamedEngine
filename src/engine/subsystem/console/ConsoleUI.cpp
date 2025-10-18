#ifdef _DEBUG
#include <pch.h>
#include <string>

#include <engine/subsystem/console/ConsoleUI.h>

#include <imgui.h>

namespace Unnamed {
	ConsoleUI::ConsoleUI(
		ConsoleSystem* consoleSystem
	) : mConsoleSystem(consoleSystem) {
		bIsImGuiInitialized = true;
		// if (ImGui::GetCurrentContext() != nullptr) {
		// 	
		// } else {
		// 	Warning(
		// 		"ConsoleUI",
		// 		"ImGuiが初期化されていないため、コンソールUIは表示されません。"
		// 	);
		// }
	}

	/// @brief コンソールUIを表示します。
	/// @details ImGuiのコンテキスト内で呼び出してください。
	void ConsoleUI::Show() const {
		if (bIsImGuiInitialized) {
			ImGui::Begin("Console##ConsoleSystem");
			for (auto buffer : mConsoleSystem->GetLogBuffer()) {
				PushLogTextColor(buffer);

				std::string text;
				if (!buffer.channel.empty()) {
					text = "[" + buffer.channel + "] " + buffer.message;
				} else {
					text = buffer.message;
				}

				ImGui::Text(text.c_str());

				ImGui::PopStyleColor();
			}
			char                inputBuffer[256] = "";
			ImGuiInputTextFlags flags            =
				ImGuiInputTextFlags_EnterReturnsTrue |
				ImGuiInputTextFlags_CallbackCompletion |
				ImGuiInputTextFlags_CallbackHistory |
				ImGuiInputTextFlags_CallbackEdit |
				ImGuiInputTextFlags_CallbackResize;

			if (
				ImGui::InputText(
					"##Input",
					inputBuffer,
					IM_ARRAYSIZE(inputBuffer),
					flags,
					InputTextCallback
				)
			) {
				Msg("Input", "{}", inputBuffer);
			}

			ImGui::End();
		}
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

	int ConsoleUI::InputTextCallback(ImGuiInputTextCallbackData* data) {
		switch (data->EventFlag) {
		case ImGuiInputTextFlags_CallbackCompletion: {
			Msg("callback", "completion");
		}
		break;

		case ImGuiInputTextFlags_CallbackHistory:
			Msg("callback", "history");
			break;

		case ImGuiInputTextFlags_CallbackEdit:
			Msg("callback", "edit");
			break;

		case ImGuiInputTextFlags_CallbackResize:
			break;
		default: ;
		}
		return 0;
	}
}
#endif
