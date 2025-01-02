#pragma once

#include <functional>
#include <string>
#include <vector>

#include "../ImGuiManager/ImGuiManager.h"

constexpr Vec4 kConsoleColorNormal = { .x = 0.87f, .y = 0.87f, .z = 0.87f, .w = 1.0f }; // 通常テキストの色
constexpr Vec4 kConsoleColorWarning = { .x = 1.0f, .y = 1.0f, .z = 0.0f, .w = 1.0f }; // 警告テキストの色
constexpr Vec4 kConsoleColorError = { .x = 1.0f, .y = 0.0f, .z = 0.0f, .w = 1.0f }; // エラーテキストの色
constexpr Vec4 kConsoleColorWait = { .x = 0.93f, .y = 0.79f, .z = 0.09f, .w = 1.0f }; // 待ち状態テキストの色
constexpr Vec4 kConsoleColorCompleted = { .x = 0.48f, .y = 0.76f, .z = 0.26f, .w = 1.0f }; // 完了テキストの色

constexpr Vec4 kConsoleColorBool = { .x = 0.58f, .y = 0.0f, .z = 0.0f, .w = 1.0f };
constexpr Vec4 kConsoleColorInt = { .x = 0.12f, .y = 0.89f, .z = 0.69f, .w = 1.0f };
constexpr Vec4 kConsoleColorFloat = { .x = 0.22f, .y = 0.84f, .z = 0.0f, .w = 1.0f };
constexpr Vec4 kConsoleColorString = { .x = 0.99f, .y = 0.0f, .z = 0.83f, .w = 1.0f };
constexpr Vec4 kConsoleColorVec3 = { .x = 1.0f, .y = 0.79f, .z = 0.14f, .w = 1.0f };

constexpr uint32_t kInputBufferSize = 512; // コンソールが一度に送信できるバッファの数
constexpr uint32_t kConsoleMaxLineCount = 2048;

constexpr uint32_t kConsoleSuggestLineCount = 8; // サジェストの最大候補数
constexpr uint32_t kConsoleRepeatWarning = 256; // リピート回数がこの数値より多くなるとkConsoleWarningで指定した色になります
constexpr uint32_t kConsoleRepeatError = 512; // リピート回数がこの数値より多くなるとkConsoleErrorで指定した色になります

using CommandCallback = std::function<void(const std::vector<std::string>&)>;

enum class Channel {
	kNone = 0, // なし

	kConsole, // コンソール

	kEngine, // エンジン

	kGeneral, // 一般
	kDeveloper, // 開発者向け

	kClient, // クライアント
	kServer, // サーバー

	kHost, // ホスト

	kGame, // ゲーム内

	kInputSystem, // 入力システム
	kSound, // サウンド
	kPhysics, // 物理
	kRenderPipeline, // レンダーパイプライン
	kRenderSystem, // レンダーシステム
	kUserInterface, // ユーザーインターフェース
};

class Console {

	struct Text {
		std::string text;
		Vec4 color;
	};

	struct SuggestPopupState {
		bool bPopupOpen; // ポップアップが開いているか?
		bool bSelectionChanged; // 選択された要素が変更されたか?
		int activeIndex; // 現在選択されている要素
		int clickedIndex; // クリックされた要素
	};

public:
	static void Update();
	static void SubmitCommand(const std::string& command);

	static void Print(const std::string& message, const Vec4& color = kConsoleColorNormal, const Channel& channel = Channel::kGeneral);
	static void PrintNullptr(const std::string& message, const Channel& channel);
	static void UpdateRepeatCount(const std::string& message, Vec4 color = kConsoleColorNormal);

	// Executable
	static void ToggleConsole(const std::vector<std::string>& args = {});
	static void Clear(const std::vector<std::string>& args = {});
	static void Help(const std::vector<std::string>& args = {});
	static void NeoFetch(const std::vector<std::string>& args = {});
	static void Echo(const std::vector<std::string>& args = {});

	static std::string ToString(const Channel& e);

private:
#ifdef _DEBUG
	static void SuggestPopup(SuggestPopupState& state, const ImVec2& pos, const ImVec2& size, bool& isFocused);
	static int InputTextCallback(ImGuiInputTextCallbackData* data);
#endif

	static void ScrollToBottom();

	static void AddCommandHistory(const std::string& command);

	static void CheckLineCount();

	static std::string TrimSpaces(const std::string& string);
	static std::vector<std::string> TokenizeCommand(const std::string& command);

#ifdef _DEBUG
	static bool bShowConsole_; // コンソールを表示するか?
	static bool bWishScrollToBottom_; // 一番下にスクロールしたい
	static bool bShowPopup_; // ポップアップを表示
	static std::vector<Text> consoleTexts_; // コンソールに出力されているテキスト
	static char inputText_[kInputBufferSize]; // 入力中のテキスト
	static int historyIndex_;
	static std::vector<std::string> history_; // 入力の履歴
	static std::vector<std::string> suggestions_; // サジェスト
	static std::vector<uint64_t> repeatCounts_;
#endif
};
