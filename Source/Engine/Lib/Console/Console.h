#pragma once

#include <condition_variable>
#include <functional>
#include <queue>
#include <string>
#include <vector>
#include <ImGuiManager/ImGuiManager.h>

constexpr Vec4 kConsoleColorNormal = Vec4(0.71f, 0.71f, 0.71f, 1.0f); // 通常テキストの色
constexpr Vec4 kConsoleColorGray = Vec4(0.77f, 0.74f, 0.66f, 1.0f); // グレーテキストの色
constexpr Vec4 kConsoleColorExecute = Vec4(0.8f, 1.0f, 1.0f, 1.0f); // コマンド実行テキストの色
constexpr Vec4 kConsoleColorWarning = Vec4(1.0f, 1.0f, 0.0f, 1.0f); // 警告テキストの色
constexpr Vec4 kConsoleColorError = Vec4(1.0f, 0.0f, 0.0f, 1.0f); // エラーテキストの色
constexpr Vec4 kConsoleColorWait = Vec4(0.93f, 0.79f, 0.09f, 1.0f); // 待ち状態テキストの色
constexpr Vec4 kConsoleColorCompleted = Vec4(0.48f, 0.76f, 0.26f, 1.0f); // 完了テキストの色

constexpr Vec4 kConsoleColorBool = Vec4(0.58f, 0.0f, 0.0f, 1.0f);
constexpr Vec4 kConsoleColorInt = Vec4(0.12f, 0.89f, 0.69f, 1.0f);
constexpr Vec4 kConsoleColorFloat = Vec4(0.22f, 0.84f, 0.0f, 1.0f);
constexpr Vec4 kConsoleColorString = Vec4(0.99f, 0.0f, 0.83f, 1.0f);
constexpr Vec4 kConsoleColorVec3 = Vec4(1.0f, 0.79f, 0.14f, 1.0f);

constexpr uint32_t kInputBufferSize = 4096; // コンソールが一度に送信できるバッファの数
constexpr uint32_t kConsoleMaxLineCount = 2048; // コンソールに表示される最大行数

constexpr uint32_t kConsoleSuggestLineCount = 8; // サジェストの最大候補数
constexpr uint32_t kConsoleRepeatWarning = 256; // リピート回数がこの数値より多くなるとkConsoleWarningで指定した色になります
constexpr uint32_t kConsoleRepeatError = 512; // リピート回数がこの数値より多くなるとkConsoleErrorで指定した色になります

using CommandCallback = std::function<void(const std::vector<std::string>&)>;

enum class Channel {
	None = 0, // なし

	CommandLine, // コマンドライン
	Console, // コンソール

	// システム基盤系
	Engine, // エンジン
	Host, // ホスト

	// システム系
	AssetSystem, // アセットシステム
	ResourceSystem, // リソースシステム
	InputSystem, // 入力システム

	// 通信系
	Client, // クライアント
	Server, // サーバー

	// ゲームロジック系
	Game, // ゲーム内
	Physics, // 物理

	// 描画・UI系
	RenderPipeline, // レンダーパイプライン
	RenderSystem, // レンダーシステム
	UserInterface, // ユーザーインターフェース
	Sound, // サウンド

	// 開発系
	General, // 一般
	Developer, // 開発者向け
};

class Console {
	struct Text {
		std::string text;
		Vec4 color;
		Channel channel;
	};

	struct SuggestPopupState {
		bool bPopupOpen; // ポップアップが開いているか?
		bool bSelectionChanged; // 選択された要素が変更されたか?
		int activeIndex; // 現在選択されている要素
		int clickedIndex; // クリックされた要素
	};

public:
	Console();
	~Console();

	static void Update();
	static void Shutdown();
	static void SubmitCommand(const std::string& command);

	static void Print(
		const std::string& message, const Vec4& color = kConsoleColorNormal, const Channel& channel = Channel::General
	);
	static void PrintNullptr(const std::string& message, const Channel& channel);

	static std::string ToString(Channel channel);

	// Executable
	static void ToggleConsole(const std::vector<std::string>& args = {});
	static void Clear(const std::vector<std::string>& args = {});
	static void Help(const std::vector<std::string>& args = {});
	static void NeoFetch(const std::vector<std::string>& args = {});
	static void Echo(const std::vector<std::string>& args = {});

private:
#ifdef _DEBUG
	static void UpdateSuggestions(const std::string& input);
	static void ShowSuggestPopup();
	static void SuggestPopup(SuggestPopupState& state, const ImVec2& pos, const ImVec2& size, bool& isFocused);
	static int InputTextCallback(ImGuiInputTextCallbackData* data);
#endif

	// ImGui系
	static void ShowMenuBar();
	static void ShowConsoleText();
	static void ShowConsoleBody();
	static void ShowContextMenu();
	static void ShowAbout();

	// 送信系
	static bool ProcessInputCommand(const std::string& command);
	static bool ValidateType(const std::string& value, const std::string& type);
	static void PrintTypeError(const std::string& type);

	static void AddCommandHistory(const std::string& command);

	static void UpdateRepeatCount(const std::string& message, bool hasNewLine, const Channel& channel, const Vec4& color = kConsoleColorNormal);

	static void CheckScroll();
	static void CheckLineCountAsync();

	static Vec4 GetConVarTypeColor(const std::string& type);

	static std::string TrimSpaces(const std::string& string);
	static std::vector<std::string> TokenizeCommand(const std::string& command);

	static size_t FilteredToActualIndex(int filteredIndex);

	static void FlushLogBuffer(const std::vector<std::string>& buffer);

	// コンソールの非同期スレッド
	void ConsoleUpdateAsync();
	void StartConsoleThread();
	static void StopConsoleThread();
	static void LogToFileAsync(const std::string& message);

	static std::mutex mutex_;
	static std::queue<std::function<void()>> taskQueue_;
	static std::condition_variable cv_;
	static std::thread consoleThread_;
	static bool bStopThread_;
	bool bConsoleUpdate_ = false;

#ifdef _DEBUG
	static bool bShowConsole_; // コンソールを表示するか?
	static bool bWishScrollToBottom_; // 一番下にスクロールしたい
	static bool bShowSuggestPopup_; // サジェストポップアップを表示
	static bool bShowAbout_; // Aboutを表示
	static std::vector<Text> consoleTexts_; // コンソールに出力されているテキスト
	static char inputText_[kInputBufferSize]; // 入力中のテキスト
	static int historyIndex_;
	static std::vector<std::string> history_; // 入力の履歴
	static std::vector<std::string> suggestions_; // サジェスト
	static std::vector<uint64_t> repeatCounts_;
	static int lastSelectedIndex_;
	static Channel currentFilterChannel_;

	// ログ
	static std::vector<std::string> messageBuffer_;
	static std::ofstream logFile_;
	static constexpr size_t kBatchSize = 4;

	struct DisplayState {
		std::vector<Text> buffer; // バッファ
		std::vector<bool> selected; // 選択状態
		size_t lastUpdateFrame = 0; // 最終更新フレーム
	};
	static DisplayState displayState_;
#endif
};
