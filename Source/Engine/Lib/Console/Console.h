#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../ImGuiManager/ImGuiManager.h"

constexpr ImVec4 kConsoleColorNormal = { 0.87f, 0.87f, 0.87f, 1.0f }; // 通常テキストの色
constexpr ImVec4 kConsoleColorWarning = { 1.0f, 1.0f, 0.0f, 1.0f }; // 警告テキストの色
constexpr ImVec4 kConsoleColorError = { 1.0f, 0.0f, 0.0f, 1.0f }; // エラーテキストの色
constexpr ImVec4 kConsoleColorWait = { 0.274f, 0.51f, 0.706f, 1.0f }; // 待ち状態テキストの色
constexpr ImVec4 kConsoleColorCompleted = { 0.35f, 0.66f, 0.41f, 1.0f }; // 完了テキストの色

constexpr ImVec4 kConsoleColorBool = { 0.58f, 0.0f, 0.0f, 1.0f };
constexpr ImVec4 kConsoleColorInt = { 0.12f, 0.89f, 0.69f, 1.0f };
constexpr ImVec4 kConsoleColorFloat = { 0.22f, 0.84f, 0.0f, 1.0f };
constexpr ImVec4 kConsoleColorString = { 0.99f, 0.0f, 0.83f, 1.0f };
constexpr ImVec4 kConsoleColorVec3 = { 1.0f, 0.79f, 0.14f, 1.0f };

constexpr uint32_t kInputBufferSize = 512; // コンソールが一度に送信できるバッファの数

constexpr uint32_t kConsoleSuggestLineCount = 8; // サジェストの最大候補数
constexpr uint32_t kConsoleRepeatWarning = 256; // リピート回数がこの数値より多くなるとkConsoleWarningで指定した色になります
constexpr uint32_t kConsoleRepeatError = 512; // リピート回数がこの数値より多くなるとkConsoleErrorで指定した色になります

struct ConsoleText {
	std::string text;
	ImVec4 color;
};

struct SuggestPopupState {
	bool bPopupOpen; // ポップアップが開いているか?
	bool bSelectionChanged; // 選択された要素が変更されたか?
	int activeIndex; // 現在選択されている要素
	int clickedIndex; // クリックされた要素
};

using CommandCallback = std::function<void(const std::vector<std::string>&)>;

class Console {
public:
	static void Update();
	static void SubmitCommand(const std::string& command);

	static void RegisterCommand(const std::string& commandName, const CommandCallback& callback);

	// Executable
	static void ToggleConsole(const std::vector<std::string>& args = {});
	static void Clear(const std::vector<std::string>& args = {});
	static void Help(const std::vector<std::string>& args = {});
	static void Neofetch(const std::vector<std::string>& args = {});

#ifdef _DEBUG
	static void Print(const std::string& message, ImVec4 color = kConsoleColorNormal);
	static void UpdateRepeatCount(const std::string& message, ImVec4 color = kConsoleColorNormal);
#else
	static void Print(const std::string& message, const ImVec4 color = { 0.0f,0.0f,0.0f,0.0f });
	static void UpdateRepeatCount(const std::string& message, ImVec4 color = { 0.0f,0.0f,0.0f,0.0f });
#endif

private:
#ifdef _DEBUG
	static void SuggestPopup(SuggestPopupState& state, const ImVec2& pos, const ImVec2& size, bool& isFocused);
	static int InputTextCallback(ImGuiInputTextCallbackData* data);
#endif

	static void ScrollToBottom();
	static float scrollAnimationSpeed;

	static void AddHistory(const std::string& command);

	static std::string TrimSpaces(const std::string& string);
	static std::vector<std::string> TokenizeCommand(const std::string& command);

#ifdef _DEBUG
	static bool bShowConsole; // コンソールを表示するか?
	static bool bWishScrollToBottom; // 一番下にスクロールしたい
	static bool bShowPopup; // ポップアップを表示
	static std::vector<ConsoleText> consoleTexts; // コンソールに出力されているテキスト
	static char inputText[kInputBufferSize]; // 入力中のテキスト
	static int historyIndex;
	static std::vector<std::string> history; // 入力の履歴
	static std::vector<std::string> suggestions; // サジェスト
	static std::vector<int> repeatCounts;
	static std::unordered_map<std::string, CommandCallback> commandMap;
#endif
};
