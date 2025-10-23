#ifdef _DEBUG
#include <pch.h>
#include <string>

#include <engine/subsystem/console/ConsoleUI.h>

#include <imgui.h>
#include <imgui_internal.h>

namespace Unnamed {
	static constexpr auto kSubmitButtonText = " Submit ";

	static constexpr ImGuiWindowFlags kWindowFlags =
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoScrollbar;

	static constexpr ImGuiInputTextFlags kInputTextFlags =
		ImGuiInputTextFlags_EnterReturnsTrue |
		ImGuiInputTextFlags_CallbackCompletion |
		ImGuiInputTextFlags_CallbackHistory |
		ImGuiInputTextFlags_CallbackEdit |
		ImGuiInputTextFlags_CallbackResize;

	/// @brief コンストラクタ
	ConsoleUI::ConsoleUI(
		ConsoleSystem* consoleSystem
	) : mConsoleSystem(consoleSystem) {
		bIsImGuiInitialized = true;
	}

	/// @brief コンソールUIを表示します。
	/// @details ImGuiのコンテキスト内で呼び出し
	void ConsoleUI::Show() {
		if (bIsImGuiInitialized) {
			ImGui::Begin("Console##ConsoleUI", nullptr, kWindowFlags);

			ImGuiChildFlags childFlags = ImGuiChildFlags_ResizeX |
				ImGuiChildFlags_FrameStyle;

			// このウィンドウで使えるサイズを取得
			auto region = ImGui::GetWindowContentRegionMax();

			float childHeight = region.y - ImGui::GetFrameHeightWithSpacing() *
				2.0f;

			ImGui::BeginChild("Output##ConsoleUI",
			                  ImVec2(region.x * 0.5f, childHeight), childFlags);

			static ConsoleLogText selection;

			for (const auto& buffer : mConsoleSystem->GetLogBuffer()) {
				PushLogTextColor(buffer);

				std::string text;
				if (!buffer.channel.empty()) {
					text = "[" + buffer.channel + "] " + buffer.message;
				} else {
					text = buffer.message;
				}

				if (ImGui::Selectable(text.c_str())) {
					selection = buffer;
				}

				ImGui::PopStyleColor();
			}

			ImGui::EndChild();

			ImGui::SameLine();

			ImGui::BeginChild("About##ConsoleUI", ImVec2(0, childHeight),
			                  ImGuiChildFlags_AlwaysUseWindowPadding |
			                  ImGuiChildFlags_FrameStyle);

			std::string bufferInfo = std::format(
				"File: {}\nLine: {}\nFunc: {}",
				selection.location.file_name(),
				selection.location.line(),
				selection.location.function_name()
			);
			ImGui::Text(
				bufferInfo.data()
			);
			ImGui::EndChild();

			DrawInputText();

			ImGui::SameLine();

			DrawSubmitButton();

			ImGui::End();
		}
	}

	/// @brief 入力テキストボックスを描画します。
	void ConsoleUI::DrawInputText() {
		auto style        = ImGui::GetStyle();
		auto spacing      = style.WindowPadding;
		auto framePadding = style.FramePadding;
		auto itemSpacing  = style.ItemSpacing;

		auto submitTextWidth = ImGui::CalcTextSize(kSubmitButtonText);

		ImVec2 size = {
			std::max(
				ImGui::GetWindowContentRegionMax().x - spacing.x -
				submitTextWidth.x - framePadding.x * 2.0f - itemSpacing.x,
				0.0f),
			ImGui::GetFrameHeightWithSpacing() - spacing.y
		};

		ImGui::InputTextEx(
			"##Input",
			nullptr,
			mInputBuffer,
			IM_ARRAYSIZE(mInputBuffer),
			size,
			kInputTextFlags,
			InputTextCallback
		);
	}

	/// @brief 送信ボタンを描画します。
	void ConsoleUI::DrawSubmitButton() {
		if (ImGui::Button(kSubmitButtonText)) {
		}
	}

	/// @brief コマンドが送信された際のイベント
	void ConsoleUI::Submit() {
		// コンソールシステムに
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

	/// @brief インプットテキストからのコールバック
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
