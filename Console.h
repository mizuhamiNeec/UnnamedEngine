#pragma once

#ifdef _DEBUG

#include <string>
#include <vector>

#include "imgui/imgui.h"

constexpr ImVec4 kConsoleNormal = { 1.0f,1.0f,1.0f,1.0f };
constexpr ImVec4 kConsoleError = { 1.0f,0.0f,0.0f,1.0f };
constexpr ImVec4 kConsoleWarning = { 1.0f,1.0f,0.0f,1.0f };

struct ConsoleText {
	std::string text;
	ImVec4 color;
};

class Console {
public:
	void Init();
	void Update();

	static void Print(const std::string& message, const ImVec4 color = kConsoleNormal);
	static void ToggleConsole();
	void OutputLog(std::string filepath, std::string log);

private:
	static void ScrollToBottom();
	static void SubmitCommand(const std::string& command);
};

static bool bShowConsole = true;
static std::vector<ConsoleText> history_;
static std::vector<int> repeatCounts_;
static bool wishScrollToBottom = false;

#endif