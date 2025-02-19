#include "DebugHud.h"

#include "../../SubSystem/Console/ConVarManager.h"
#include "../Timer/EngineTimer.h"

#include "ImGuiManager/ImGuiManager.h"

void DebugHud::Update() {
	ShowFrameRate();
	ShowPlayerInfo();
}

void DebugHud::ShowFrameRate() {
#ifdef _DEBUG
	const int flag = ConVarManager::GetConVar("cl_showfps")->GetValueAsInt();

	if (flag == 0) {
		return;
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });

	constexpr ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoTitleBar |
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav;

	auto windowPos = ImVec2(0.0f, 128.0f);

	windowPos.x = ImGui::GetMainViewport()->Pos.x + windowPos.x;
	windowPos.y = ImGui::GetMainViewport()->Pos.y + windowPos.y;

	ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always);
	std::string text;
	float fps;
	if (flag == 1) {
		fps = 1.0f / EngineTimer::GetScaledDeltaTime();
	}
	if (flag == 2) {
		ImGuiIO io = ImGui::GetIO();
		fps = io.Framerate;
	}

	text = std::format("{:.2f} fps", fps);

	const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

	// ウィンドウサイズをテキストサイズに基づいて設定
	const auto windowSize = ImVec2(textSize.x + 20.0f, textSize.y + 20.0f); // 余白を追加
	ImGui::SetNextWindowSize(windowSize, ImGuiCond_Always);

	ImGui::Begin("##cl_showfps", nullptr, windowFlags);

	ImVec2 textPos = ImGui::GetCursorScreenPos();

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	float outlineSize = 1.0f;

	ImVec4 textColor = ToImVec4(kConTextColorError);
	if (fps >= 59.9f) {
		textColor = ToImVec4(kConsoleColorFloat);
	} else if (fps >= 29.9f) {
		textColor = ToImVec4(kConTextColorWarning);
	}

	ImGuiManager::TextOutlined(
		drawList,
		textPos,
		text.c_str(),
		textColor,
		ToImVec4(kDebugHudOutlineColor),
		outlineSize
	);

	ImGui::PopStyleVar();

	ImGui::End();
#endif
}

void DebugHud::ShowPlayerInfo() {
}
