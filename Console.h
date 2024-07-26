#pragma once

#ifdef _DEBUG

#include <string>
#include <vector>

#include "imgui/imgui.h"

constexpr ImVec4 kConsoleNormal = { 1.0f,1.0f,1.0f,1.0f }; // 通常テキストの色
constexpr ImVec4 kConsoleWarning = { 1.0f,1.0f,0.0f,1.0f }; // 警告テキストの色
constexpr ImVec4 kConsoleError = { 1.0f,0.0f,0.0f,1.0f }; // エラーテキストの色

constexpr uint32_t kInputBufferSize = 512; // コンソールが一度に送信できるバッファの数

constexpr uint32_t kConsoleSuggestLineCount = 8; // サジェストの最大候補数
constexpr uint32_t kConsoleRepeatWarning = 256; // リピート回数がこの数値より多くなるとkConsoleWarningで指定した色になります
constexpr uint32_t kConsoleRepeatError = 512; // リピート回数がこの数値より多くなるとkConsoleErrorで指定した色になります


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

	static void UpdateRepeatCount(const std::string& message, ImVec4 color = kConsoleNormal);

private:
	static void ScrollToBottom();
	static void SubmitCommand(const std::string& command);
	static void AddHistory(const std::string& command);
	static void ShowPopup();

	static std::string TrimSpaces(const std::string& string); // 余分なスペースを取り除きます
	static std::vector<std::string> TokenizeCommand(const std::string& command);
};

static bool bShowConsole = false; // コンソールを表示するか?
static bool wishScrollToBottom = false; // 一番下にスクロールしたい
static bool bShowPopup = false; // ポップアップを表示
static std::vector<ConsoleText> consoleTexts; // コンソールに出力されているテキスト
static char str[kInputBufferSize] = { 0 }; // 入力中のテキスト
static int historyIndex = -1;
static std::vector<std::string> history; // 入力の履歴
static std::vector<int> repeatCounts;
#endif
