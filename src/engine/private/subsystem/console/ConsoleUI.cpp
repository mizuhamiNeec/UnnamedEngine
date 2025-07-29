#include <string>

#include <engine/public/subsystem/console/ConsoleUI.h>
#include <engine/public/subsystem/console/Log.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

namespace Unnamed {
	ConsoleUI::ConsoleUI(
		ConsoleSystem* consoleSystem
	): mConsoleSystem(consoleSystem) {
		Msg(
			"ConsoleSystem",
			"Console UI Initialized!"
		);
	}

	void ConsoleUI::Show() {
		ImGui::Begin("Console##ConsoleSystem");
		for (auto buffer : mConsoleSystem->GetLogBuffer()) {
			ImGui::Text(
				(buffer.message + " " + std::to_string(
					mConsoleSystem->GetLogBuffer().LastWrittenIndex())).
				c_str());
		}
		ImGui::End();
	}
}
