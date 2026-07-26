#pragma once
#ifdef UNNAMED_WITH_EDITOR
#include <string>
#include <variant>
#include <vector>

namespace Unnamed {
	class ConsoleSystem;

	/// @brief コンソール変数ヘルパークラス
	class ConVarHelper {
	public:
		explicit ConVarHelper(ConsoleSystem* consoleSystem) :
			mConsoleSystem(
				consoleSystem
			) {
		}

		/// @brief ConVarヘルパーUIを表示します
		void Show(bool& showConVarHelper);

	private:
		/// @brief メニューバーを表示します
		void ShowMenuBar();

		/// @brief ページ選択を表示します
		void ShowPageSelect();

		/// @brief グリッドサイズコントロールを表示します
		void ShowGridSizeControls();

		/// @brief グリッドを表示します
		void ShowGrid();

		// 要素編集
		/// @brief 要素編集ポップアップを表示します
		void ShowElementEditPopup();
		void LabelEdit();            // ラベル編集
		void ButtonEdit();           // ボタン編集
		void ExecutableButtonEdit(); // 実行可能ボタン編集

		void ImportPage();
		void ExportPage();

		void RearrangeGridElements(uint32_t newWidth, uint32_t newHeight);

		ConsoleSystem* mConsoleSystem = nullptr;

		static constexpr auto kBgColorDefault = Vec4(0.29f, 0.29f, 0.29f, 1.0f);
		static constexpr auto kFgColorDefault = Vec4(0.71f, 0.71f, 0.72f, 1.0f);

		/// @brief Emptyは、ConVar UI layout variantで内容を持たない空要素を表します
		struct Empty {
		};

		/// @brief ラベルの構造体
		/// @details 区切りや説明用のラベル
		struct Label {
			std::string label;                     // ラベル名
			Vec4        bgColor = kBgColorDefault; // 背景色
			Vec4        fgColor = kFgColorDefault; // 前景色
			std::string description;               // 説明
		};

		/// @brief ボタンの構造体
		/// @details ConVar/ConCommandを実行するボタン
		struct Button {
			std::string label;                     // ボタン名
			std::string command;                   // 実行するコマンド
			Vec4        bgColor = kBgColorDefault; // 背景色
			Vec4        fgColor = kFgColorDefault; // 前景色
			std::string description;               // 説明
		};

		/// @brief 実行可能ボタンの構造体
		/// @details exe/bat/cmd/etc.を実行するボタン
		struct ExecutableButton {
			std::string label;                     // ラベル名
			std::string command;                   // 実行するexe/bat/cmd/etc.
			std::string arguments;                 // 引数
			Vec4        bgColor = kBgColorDefault; // 背景色
			Vec4        fgColor = kFgColorDefault; // 前景色
			std::string description;               // 説明
		};

		/// @brief グリッド要素の構造体
		struct GridElement {
			std::variant<Empty, Label, Button, ExecutableButton> element;
		};

		/// @brief Pageは、ConVar helper UIのpage名と所属control列を保持します
		struct Page {
			std::string name; // ページ名

			/// @brief Gridは、ConVar helper UIを行列配置する列数、間隔、control列を保持します
			struct Grid {
				uint32_t width;            // テーブルの列数
				uint32_t height;           // テーブルの行数
			}                        grid; // グリッドサイズ
			std::vector<GridElement> elements;
		};

		// ページ (空で4ページ用意しておく)
		std::vector<Page> mPages = {
			{
				.name = "User Page 1",
				.grid{.width = 3, .height = 10},
				.elements = {}
			},
			{
				.name = "User Page 2",
				.grid{.width = 3, .height = 10},
				.elements = {}
			},
			{
				.name = "User Page 3",
				.grid{.width = 3, .height = 10},
				.elements = {}
			},
			{
				.name = "User Page 4",
				.grid{.width = 3, .height = 10},
				.elements = {}
			}
		}; // ページ

		size_t   mEditingElementIndex = 0;
		uint32_t mSelectedPageIndex   = 0;
		bool     mShowElementPopup    = false;

		bool        mElementEditInitialized = false;
		GridElement mElementEditOriginal{};
		GridElement mElementEditWorking{};

		// 下書き
		ExecutableButton mElementEditDraftExecutableButton{};
		std::string      mElementEditDraftButtonCommand{};
	};
}
#endif
