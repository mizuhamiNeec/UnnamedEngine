#pragma once
#ifdef UNNAMED_WITH_EDITOR
#include <string>
#include <string_view>
#include <vector>

#include "core/math/Vec4.h"
struct ImVec2;
struct ImGuiInputTextCallbackData;

namespace Unnamed {
	struct ConsoleLogText;
	class ConVarHelper;
	class ConCommand;
	class ConsoleSystem;

	constexpr Vec4 kConTextColor        = {0.71f, 0.71f, 0.72f, 1.0f};
	constexpr Vec4 kConTextColorDev     = {0.18f, 0.55f, 0.18f, 1.0f};
	constexpr Vec4 kConTextColorWarn    = {0.93f, 0.79f, 0.09f, 1.0f};
	constexpr Vec4 kConTextColorError   = {0.71f, 0.25f, 0.25f, 1.0f};
	constexpr Vec4 kConTextColorFatal   = {0.71f, 0.0f, 0.0f, 1.0f};
	constexpr Vec4 kConTextColorExec    = {0.8f, 1.0f, 1.0f, 1.0f};
	constexpr Vec4 kConTextColorWait    = {0.93f, 0.79f, 0.09f, 1.0f};
	constexpr Vec4 kConTextColorSuccess = {0.48f, 0.76f, 0.26f, 1.0f};
	constexpr Vec4 kConTextColorBool    = {0.58f, 0.0f, 0.0f, 1.0f};
	constexpr Vec4 kConTextColorInt     = {0.12f, 0.88f, 0.68f, 1.0f};
	constexpr Vec4 kConTextColorFloat   = {0.22f, 0.83f, 0.0f, 1.0f};
	constexpr Vec4 kConTextColorDouble  = {0.12f, 0.47f, 0.0f, 1.0f};
	constexpr Vec4 kConTextColorString  = {0.99f, 0.0f, 0.82f, 1.0f};
	constexpr Vec4 kConTextColorVec3    = {0.99f, 0.78f, 0.14f, 1.0f};

	struct InputTextWithComboItems {
		const char* (*itemGetter)(void* userData, int idx);
		void*         userData;
		int           itemCount;
	};

	/// @brief コンソールUIクラス
	class ConsoleUI {
	public:
		explicit ConsoleUI(ConsoleSystem* consoleSystem);
		~ConsoleUI();

		// コピー禁止
		ConsoleUI(const ConsoleUI&)            = delete;
		ConsoleUI& operator=(const ConsoleUI&) = delete;

		/// @brief 初期化
		void Init();

		/// @brief コンソールUIを表示します
		void Show();

		/// @brief コンソールが更新された際のイベント
		void OnConsoleUpdate();

	private:
		struct SuggestionContext {
			size_t      segmentStart = 0;
			size_t      tokenStart   = 0;
			size_t      tokenEnd     = 0;
			std::string token;
			bool        tokenConfirmed = false;
		};

		struct SuggestionItem {
			std::string name;
			std::string secondaryText;
			bool        isConVar = false;
		};

		/// @brief メニューバーを表示します
		void ShowMenuBar();

		/// @brief 入力欄と送信ボタンを描画します
		void DrawInputTextAndSubmitButton();

		/// @brief コンソールのコンテキストメニューを表示します
		void ShowContextMenu() const;

		/// @brief 入力されたコマンドを送信します
		void Submit();

		/// @brief スクロール状態をチェックし、自動スクロールを行います
		void CheckScroll();

		/// @brief コンソールログのテキストの色を設定します。
		/// @param buffer コンソールログのテキスト情報
		static void PushTextColor(const ConsoleLogText& buffer);

		/// @brief 指定されたファイルパスと行番号で外部エディタを開きます
		/// @param file ファイルパス
		/// @param line 行番号
		/// @param column 列番号
		static void OpenSourceFile(
			const std::string& file, int line, int column
		);

		void UpdateSuggestions(std::string_view input) const;
		void DrawSuggestionsPopup(
			const ImVec2& inputLeftTop, float inputWidth, bool inputActive
		);
		static void MoveSuggestionSelection(int delta);
		void        ApplySuggestionToInputBuffer(
			const std::string& suggestion, bool appendSpace = false
		);
		static void ApplySuggestionToCallbackBuffer(
			ImGuiInputTextCallbackData* data, const std::string& suggestion,
			bool                        appendSpace = false
		);
		static SuggestionContext ParseSuggestionContext(std::string_view input);
		std::vector<SuggestionItem> BuildSuggestionsForToken(
			std::string_view token
		) const;

		bool InputTextWithCombo(
			const char*              label, char* buf, size_t bufSize,
			InputTextWithComboItems* items
		);

		static int InputTextCallback(ImGuiInputTextCallbackData* data);

		static size_t FilteredToActualIndex(int filteredIndex);

		// 表示
		/// @brief ログテーブル（ヘッダ+行）を描画します
		void DrawLogTable(const ImVec2& childSize);
		/// @brief ログ行を描画します（フィルタ/選択/右クリックも含む）
		void DrawLogRows(int& visibleIndex, bool& requestOpenContextMenu) const;
		/// @brief 1行分の描画と入力処理を行います。描画した場合は true
		bool DrawLogRow(
			size_t actualIndex, int visibleIndex, bool& requestOpenContextMenu
		) const;
		/// @brief 選択中ログをクリップボードへコピーします（1要素=1行）
		void CopySelectedToClipboard() const;
		/// @brief Ctrl+C のコピー処理
		void HandleCopyShortcut() const;
		/// @brief 右クリックで開くコンテキストメニューを要求します
		static void OpenConsoleContextMenuPopup();

		ConsoleSystem* mConsoleSystem; // コンソールシステムへのポインタ

		std::unique_ptr<ConVarHelper> mConVarHelper; // ConVarヘルパー
		std::unique_ptr<ConCommand>   mToggleConsoleCommand;

		bool mShowConsole      = true;  // コンソール表示フラグ
		bool mShowAbout        = false; // Aboutウィンドウ表示フラグ
		bool mShowConVarHelper = false; // ConVarヘルパー表示フラグ

		bool mWishScrollToBottom = false; // 自動スクロールフラグ

		char mInputBuffer[256] = "";
	};
}
#endif
