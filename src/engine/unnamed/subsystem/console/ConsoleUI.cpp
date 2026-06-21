#if defined(UNNAMED_WITH_EDITOR)
#include <algorithm>
#include <imgui.h>
#include <imgui_internal.h>
#include <pch.h>
#include <string>
#include <unordered_set>
#include <utility>

#include <core/math/Vec4.h>
#include <core/string/StrUtil.h>

#include <engine/Properties.h>
#include <engine/ImGui/Icons.h>
#include <engine/ImGui/ImGuiWidgets.h>
#include <engine/unnamed/subsystem/console/ConsoleUI.h>
#include <engine/unnamed/subsystem/console/ConVarHelper.h>
#include <engine/unnamed/subsystem/console/concommand/ConCommand.h>

namespace Unnamed {
	static constexpr uint32_t kHistoryBufferSize = 64;
	static constexpr auto kConsoleUIContextPopupId = "##ConsoleUIContextMenu";

	namespace {
		/// @brief コンソールUIのデータ構造体
		struct ConsoleUIData {
			struct SuggestionEntry {
				std::string name;
				std::string secondaryText;
				bool        isConVar = false;
			};

			struct SuggestionState {
				std::vector<SuggestionEntry> items;
				std::string                  query;
				int                          selectedIndex = -1;
			};

			RingBuffer<std::string, kHistoryBufferSize> history = {};

			// 選択
			std::vector<bool> selected;
			int               lastSelectedFilteredIndex = -1;

			// フィルタ
			std::string currentFilterChannel;

			// 履歴ナビゲーション
			std::string scratch      = {}; // 一時保存用
			int         historyIndex = -1;
			bool        browsing     = false;

			// クリッピング用: フィルタ後の表示行 -> 実ログ(actualIndex) の対応
			std::vector<size_t> filteredToActual = {};

			// サジェスト状態
			SuggestionState suggestion                      = {};
			bool            requestInputRefocus             = false;
			bool            requestMoveCursorToEnd          = false;
			bool            suppressSuggestionRefreshOnEdit = false;
		} gConsoleUIData;
	}

	static constexpr auto kSubmitButtonText = " Submit ";

	static constexpr ImGuiWindowFlags kWindowFlags =
		ImGuiWindowFlags_NoScrollWithMouse |
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_MenuBar;

	static constexpr ImGuiInputTextFlags kInputTextFlags =
		ImGuiInputTextFlags_EnterReturnsTrue |
		ImGuiInputTextFlags_CallbackCompletion |
		ImGuiInputTextFlags_CallbackHistory |
		ImGuiInputTextFlags_CallbackEdit |
		ImGuiInputTextFlags_CallbackResize;

	static constexpr int kMaxSuggestionDisplayCount = 8;

	namespace {
		std::string ToLowerAscii(const std::string_view text) {
			std::string lowered(text);
			std::ranges::transform(
				lowered,
				lowered.begin(),
				[](const unsigned char c) {
					return static_cast<char>(std::tolower(c));
				}
			);
			return lowered;
		}

		size_t FindFirstCaseInsensitive(
			const std::string_view text, const std::string_view query
		) {
			if (query.empty() || text.empty()) {
				return std::string::npos;
			}
			const auto loweredText  = ToLowerAscii(text);
			const auto loweredQuery = ToLowerAscii(query);
			return loweredText.find(loweredQuery);
		}

		int CountDisplayLines(const std::string_view text) {
			int count = 1;
			for (const char c : text) {
				if (c == '\n') {
					++count;
				}
			}
			return std::max(1, count);
		}
	}

	/// @brief コンストラクタ
	ConsoleUI::ConsoleUI(
		ConsoleSystem* consoleSystem
	) : mConsoleSystem(consoleSystem) {
		// ConVarヘルパーを初期化
		mConVarHelper = std::make_unique<ConVarHelper>(consoleSystem);
	}

	ConsoleUI::~ConsoleUI() {
		mToggleConsoleCommand.reset();
	};

	void ConsoleUI::Init() {
		mToggleConsoleCommand = std::make_unique<ConCommand>(
			"toggleconsole",
			[&](const std::vector<std::string>&) {
				mShowConsole = !mShowConsole;
				return true;
			},
			"Show/hide the console."
		);
	}

	/// @brief コンソールUIを表示します。
	/// @details ImGuiのコンテキスト内で呼び出し
	void ConsoleUI::Show() {
		// ConVarヘルパーは独立して表示
		if (mConVarHelper) {
			mConVarHelper->Show(mShowConVarHelper);
		}

		if (!mShowConsole) {
			return;
		}

		ImGui::SetNextWindowSizeConstraints(
			ImVec2(320, 320), ImVec2(FLT_MAX, FLT_MAX)
		);

		const bool windowOpen = ImGui::Begin(
			"Console##ConsoleUI",
			&mShowConsole,
			kWindowFlags
		);

		if (windowOpen) {
			ShowMenuBar();

			// 入力テキストの高さと子ウィンドウのサイズを計算
			const float inputTextHeight = ImGui::GetFrameHeightWithSpacing();
			const auto  childSize       = ImVec2(
				0, -inputTextHeight - ImGuiStyle().ItemSpacing.y
			);

			DrawLogTable(childSize);

			DrawInputTextAndSubmitButton();

			if (mShowAbout) {
				ImGuiWidgets::ShowAboutWindow(
					"Unnamed Console", std::string(ENGINE_VERSION),
					kIconTerminal,
					mShowAbout
				);
			}
		}

		ImGui::End();
	}

	void ConsoleUI::OnConsoleUpdate() {
		mWishScrollToBottom = true;
	}

	void ConsoleUI::ShowMenuBar() {
		if (ImGui::BeginMenuBar()) {
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding,
				ImVec2(kPopupPadding, kPopupPadding)
			);

			if (ImGui::BeginMenu("File")) {
				if (ImGuiWidgets::MenuItemWithIcon("Clear", kIconReset)) {
					// コンソール出力をクリア
					mConsoleSystem->ExecuteCommand("clear");
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("コンソールの履歴をクリアします。");
					ImGui::EndTooltip();
				}

				ImGui::Separator();

				if (ImGuiWidgets::MenuItemWithIcon("Close", kIconClose)) {
					// コンソールを閉じる
					mShowConsole = false;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("コンソールを閉じます");
					ImGui::EndTooltip();
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Tools")) {
				if (ImGuiWidgets::MenuItemWithIcon(
					"ConVar Helper", kIconSettings, nullptr, mShowConVarHelper
				)) {
					// ConVarヘルパーを表示/非表示
					mShowConVarHelper = !mShowConVarHelper;
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("ConVarヘルパーウィンドウを表示/非表示します。");
					ImGui::EndTooltip();
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help")) {
				if (ImGuiWidgets::MenuItemWithIcon("Help", kIconHelp)) {
					mConsoleSystem->ExecuteCommand("help");
				}
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("ヘルプを表示します。");
					ImGui::EndTooltip();
				}

				if (
					ImGuiWidgets::MenuItemWithIcon(
						"About", kIconInfo, nullptr, mShowAbout
					)
				) {
					mShowAbout = true;
				}

				if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal)) {
					ImGui::BeginTooltip();
					ImGui::TextUnformatted("コンソールUIについて。");
					ImGui::EndTooltip();
				}
				ImGui::EndMenu();
			}

			ImGui::PopStyleVar();

			ImGui::EndMenuBar();
		}
	}

	void ConsoleUI::DrawInputTextAndSubmitButton() {
		const ImGuiStyle& style   = ImGui::GetStyle();
		const float       availW  = ImGui::GetContentRegionAvail().x;
		const float       submitW =
			ImGui::CalcTextSize(kSubmitButtonText).x +
			style.FramePadding.x *
			2.0f;
		const float spacingW = style.ItemSpacing.x;

		const float  inputW = std::max(availW - submitW - spacingW, 0.0f);
		const ImVec2 inputSize{inputW, 0.0f}; // 高さは任せる

		// 下端に寄せる
		const float lineH   = ImGui::GetFrameHeightWithSpacing();
		const float remainH = ImGui::GetContentRegionAvail().y;
		if (remainH > lineH) {
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (remainH - lineH));
		}

		ImGui::SetNextItemWidth(inputW);

		// 入力テキストの左上座標を取る
		const ImVec2 textLeftTop = ImGui::GetCursorScreenPos();

		const bool enterPressed = ImGui::InputTextEx(
			"##Input",
			nullptr,
			mInputBuffer,
			IM_ARRAYSIZE(mInputBuffer),
			inputSize,
			kInputTextFlags,
			InputTextCallback,
			this
		);
		const ImGuiID inputId = ImGui::GetItemID();

		if (gConsoleUIData.requestInputRefocus) {
			ImGui::SetKeyboardFocusHere(-1);
		}
		if (gConsoleUIData.requestMoveCursorToEnd) {
			if (auto* state = ImGui::GetInputTextState(inputId)) {
				state->ReloadUserBufAndMoveToEnd();
				state->ClearSelection();
				gConsoleUIData.requestMoveCursorToEnd = false;
				gConsoleUIData.requestInputRefocus    = false;
			}
		}

		const bool inputActive =
			ImGui::IsItemActive() || ImGui::IsItemFocused();
		const auto& suggestionState        = gConsoleUIData.suggestion;
		const bool  hasSuggestionSelection =
			inputActive &&
			!suggestionState.items.empty() &&
			suggestionState.selectedIndex >= 0 &&
			std::cmp_less(
				suggestionState.selectedIndex, suggestionState.items.size()
			);

		if (enterPressed) {
			if (hasSuggestionSelection) {
				const auto& selected =
					suggestionState.items[static_cast<size_t>(
						suggestionState.selectedIndex
					)];
				ApplySuggestionToInputBuffer(selected.name, true);
				UpdateSuggestions(mInputBuffer);
				gConsoleUIData.requestInputRefocus    = true;
				gConsoleUIData.requestMoveCursorToEnd = true;
			} else {
				Submit();
			}
		}

		DrawSuggestionsPopup(textLeftTop, inputW, inputActive);

		ImGui::SameLine(0.0f, spacingW);

		if (ImGui::Button(kSubmitButtonText)) {
			Submit();
		}
	}

	void ConsoleUI::ShowContextMenu() const {
		ImGui::PushStyleVar(
			ImGuiStyleVar_WindowPadding,
			ImVec2(kPopupPadding, kPopupPadding)
		);

		if (ImGui::BeginPopup(kConsoleUIContextPopupId)) {
			// 選択数をカウント
			int    selectedCount = 0;
			size_t selectedIndex = 0;
			for (size_t i = 0; i < gConsoleUIData.selected.size(); ++i) {
				if (gConsoleUIData.selected[i]) {
					selectedCount++;
					selectedIndex = i;
				}
			}

			if (selectedCount == 1) {
				// 単一選択時のみ、ソース位置と「Open Source File」を表示
				const auto& buffer = mConsoleSystem->GetLogBuffer()[
					selectedIndex];
				ImGui::TextDisabled(
					"File: %s", buffer.location.file_name()
				);
				ImGui::TextDisabled("Line: %u", buffer.location.line());

				if (ImGuiWidgets::MenuItemWithIcon(
					"Open Source File",
					kIconBox
				)) {
					OpenSourceFile(
						buffer.location.file_name(),
						static_cast<int>(buffer.location.line()),
						static_cast<int>(buffer.location.column())
					);
				}
				ImGui::Separator();
			}

			// 複数選択時は「Copy Selected」のみ表示
			if (
				ImGuiWidgets::MenuItemWithIcon(
					"Copy Selected",
					kIconCopy
				)
			) {
				CopySelectedToClipboard();
			}

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();
	}

	void ConsoleUI::Submit() {
		gConsoleUIData.requestInputRefocus    = false;
		gConsoleUIData.requestMoveCursorToEnd = false;

		// 送信してもフォーカスは維持
		ImGui::SetKeyboardFocusHere(-1);

		const auto cmd = std::string(mInputBuffer);

		// 空は履歴に入れない（実行は ConsoleSystem 側に任せる）
		if (!cmd.empty()) {
			gConsoleUIData.history.Push(cmd);
		}

		// 履歴ナビゲーションの状態をリセット
		gConsoleUIData.historyIndex =
			static_cast<int>(gConsoleUIData.history.Size());
		gConsoleUIData.scratch.clear();
		gConsoleUIData.browsing = false;

		// コンソールシステムに送る
		mConsoleSystem->ExecuteCommand(cmd);
		mInputBuffer[0] = '\0';
		gConsoleUIData.suggestion.items.clear();
		gConsoleUIData.suggestion.query.clear();
		gConsoleUIData.suggestion.selectedIndex        = -1;
		gConsoleUIData.suppressSuggestionRefreshOnEdit = false;

		// コマンドが送信時に一番下までスクロールする
		mWishScrollToBottom = true;
	}

	void ConsoleUI::CheckScroll() {
		// スクロールが一番下にある場合、自動スクロールを行う
		if (
			mWishScrollToBottom &&
			ImGui::GetScrollY()
			>=
			ImGui::GetScrollMaxY()
		) {
			ImGui::SetScrollHereY(1.0f);
		}

		// スクロール位置をチェックし、状態を更新
		if (ImGui::GetScrollY() < ImGui::GetScrollMaxY()) {
			mWishScrollToBottom = false;
		} else {
			mWishScrollToBottom = true;
		}
	}

	void ConsoleUI::PushTextColor(const ConsoleLogText& buffer) {
		switch (buffer.level) {
			case LogLevel::None:
			case LogLevel::Info: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColor)
				);
				break;
			case LogLevel::Dev: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorDev)
				);
				break;
			case LogLevel::Warning: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorWarn)
				);
				break;
			case LogLevel::Error: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorError)
				);
				break;
			case LogLevel::Fatal: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorFatal)
				);
				break;
			case LogLevel::Execute: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorExec)
				);
				break;
			case LogLevel::Waiting: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorWait)
				);
				break;
			case LogLevel::Success: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorSuccess)
				);
				break;
			case LogLevel::Bool: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorBool)
				);
				break;
			case LogLevel::Int: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorInt)
				);
				break;
			case LogLevel::Float: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorFloat)
				);
				break;
			case LogLevel::Double: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorDouble)
				);
				break;
			case LogLevel::String: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorString)
				);
				break;
			case LogLevel::Vec3: ImGui::PushStyleColor(
					ImGuiCol_Text, ToImVec4(kConTextColorVec3)
				);
				break;
		}
	}

	void ConsoleUI::OpenSourceFile(
		const std::string& file, int line, int column
	) {
		const std::string command = std::format(
			"--line {} --column {} {}",
			line,
			column,
			file
		);
		ShellExecuteW(
			nullptr,
			L"open",
			L"Rider.cmd",
			StrUtil::ToWString(command).c_str(),
			nullptr,
			SW_HIDE
		);
	}

	void ConsoleUI::UpdateSuggestions(const std::string_view input) const {
		auto&       state = gConsoleUIData.suggestion;
		std::string prevSelectedName;
		if (
			state.selectedIndex >= 0 &&
			std::cmp_less(state.selectedIndex, state.items.size())
		) {
			prevSelectedName = state.items[static_cast<size_t>(state.
					selectedIndex)].
				name;
		}

		state.items.clear();
		state.query.clear();
		state.selectedIndex = -1;

		if (input.empty()) {
			return;
		}

		const auto ctx = ParseSuggestionContext(input);
		if (ctx.tokenConfirmed) {
			return;
		}
		if (!ctx.token.empty()) {
			const std::string lowerToken             = ToLowerAscii(ctx.token);
			auto              IsCaseInsensitiveExact = [&](
				const std::vector<std::string>& names
			) {
				return std::ranges::any_of(
					names,
					[&](const std::string& name) {
						return ToLowerAscii(name) == lowerToken;
					}
				);
			};

			constexpr size_t exactCheckCount = kMaxSuggestionDisplayCount * 2;
			const bool       hasExactVar     = IsCaseInsensitiveExact(
				mConsoleSystem->FindSimilarConVars(ctx.token, exactCheckCount)
			);
			const bool hasExactCmd = IsCaseInsensitiveExact(
				mConsoleSystem->FindSimilarConCommands(
					ctx.token, exactCheckCount
				)
			);
			if (hasExactVar || hasExactCmd) {
				return;
			}
		}
		state.query = ctx.token;

		auto suggestions = BuildSuggestionsForToken(ctx.token);
		if (suggestions.empty()) {
			return;
		}

		state.items.reserve(suggestions.size());
		for (auto& suggestion : suggestions) {
			ConsoleUIData::SuggestionEntry entry;
			entry.name          = std::move(suggestion.name);
			entry.secondaryText = std::move(suggestion.secondaryText);
			entry.isConVar      = suggestion.isConVar;
			state.items.push_back(std::move(entry));
		}

		if (!prevSelectedName.empty()) {
			for (size_t i = 0; i < state.items.size(); ++i) {
				if (state.items[i].name == prevSelectedName) {
					state.selectedIndex = static_cast<int>(i);
					break;
				}
			}
		}

		if (state.selectedIndex == -1 && !ctx.token.empty()) {
			for (size_t i = 0; i < state.items.size(); ++i) {
				if (state.items[i].name == ctx.token) {
					state.selectedIndex = static_cast<int>(i);
					break;
				}
			}
		}

		if (state.selectedIndex == -1) {
			state.selectedIndex = 0;
		}
	}

	void ConsoleUI::DrawSuggestionsPopup(
		const ImVec2& inputLeftTop, const float inputWidth,
		const bool    inputActive
	) {
		const auto& state = gConsoleUIData.suggestion;
		if (!inputActive || state.items.empty()) {
			return;
		}

		const int itemCount = std::min(
			static_cast<int>(state.items.size()),
			kMaxSuggestionDisplayCount
		);
		if (itemCount <= 0) {
			return;
		}

		const float rowHeight = ImGui::GetFrameHeight();
		const float popupH    =
			rowHeight * static_cast<float>(itemCount) + kPopupPadding * 2.0f;
		const auto popupPos = ImVec2(
			inputLeftTop.x,
			inputLeftTop.y - popupH - kPopupPadding
		);
		const auto popupSize = ImVec2(std::max(inputWidth, 200.0f), popupH);

		ImGui::SetNextWindowPos(popupPos);
		ImGui::SetNextWindowSize(popupSize);
		ImGui::PushStyleVar(
			ImGuiStyleVar_WindowPadding,
			ImVec2(kPopupPadding, kPopupPadding)
		);

		constexpr ImGuiWindowFlags flags =
			ImGuiWindowFlags_NoFocusOnAppearing |
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoDecoration |
			ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoNavFocus |
			ImGuiWindowFlags_NoDocking;

		if (ImGui::Begin("##ConsoleSuggestWindow", nullptr, flags)) {
			ImGui::PushStyleVar(
				ImGuiStyleVar_ItemSpacing,
				ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f)
			);
			for (int visualIndex = 0; visualIndex < itemCount; ++visualIndex) {
				const int         i = itemCount - 1 - visualIndex;
				const bool        isSelected = i == state.selectedIndex;
				const auto&       item = state.items[static_cast<size_t>(i)];
				const std::string rowId =
					"##ConsoleSuggestRow_" + std::to_string(i);
				const float rowWidth = std::max(
					ImGui::GetContentRegionAvail().x, 0.0f
				);

				if (ImGuiWidgets::SelectableWithRounding(
					rowId.c_str(),
					isSelected,
					ImGuiSelectableFlags_AllowOverlap,
					ImVec2(rowWidth, rowHeight),
					4.0f
				)) {
					gConsoleUIData.suggestion.selectedIndex = i;
					ApplySuggestionToInputBuffer(item.name);
					gConsoleUIData.requestInputRefocus    = true;
					gConsoleUIData.requestMoveCursorToEnd = true;
				}

				const ImVec2 rowMin = ImGui::GetItemRectMin();
				const ImVec2 rowMax = ImGui::GetItemRectMax();
				const float  textY  =
					rowMin.y + ImGui::GetStyle().FramePadding.y;
				const float textX = rowMin.x + ImGui::GetStyle().FramePadding.x;
				const auto  secondaryColor = ImGui::GetColorU32(
					ImGuiCol_TextDisabled
				);
				const auto normalColor = ImGui::GetColorU32(ImGuiCol_Text);
				const auto matchColor  = ImGui::GetColorU32(
					ImGuiCol_PlotHistogram
				);
				auto* drawList = ImGui::GetWindowDrawList();

				const size_t matchPos = FindFirstCaseInsensitive(
					item.name,
					state.query
				);
				if (
					matchPos == std::string::npos || state.query.empty() ||
					matchPos >= item.name.size()
				) {
					drawList->AddText(
						ImVec2(textX, textY),
						normalColor,
						item.name.c_str()
					);
				} else {
					const size_t matchLen = std::min(
						state.query.size(),
						item.name.size() - matchPos
					);
					const std::string prefix = item.name.substr(0, matchPos);
					const std::string match  = item.name.substr(
						matchPos, matchLen
					);
					const std::string suffix = item.name.substr(
						matchPos + matchLen
					);

					float x = textX;
					if (!prefix.empty()) {
						drawList->AddText(
							ImVec2(x, textY),
							normalColor,
							prefix.c_str()
						);
						x += ImGui::CalcTextSize(prefix.c_str()).x;
					}
					if (!match.empty()) {
						drawList->AddText(
							ImVec2(x, textY),
							matchColor,
							match.c_str()
						);
						x += ImGui::CalcTextSize(match.c_str()).x;
					}
					if (!suffix.empty()) {
						drawList->AddText(
							ImVec2(x, textY),
							normalColor,
							suffix.c_str()
						);
					}
				}

				if (!item.secondaryText.empty()) {
					const ImVec2 secondarySize = ImGui::CalcTextSize(
						item.secondaryText.c_str()
					);
					const auto secondaryPos = ImVec2(
						rowMax.x - ImGui::GetStyle().FramePadding.x -
						secondarySize.x,
						textY
					);
					drawList->AddText(
						secondaryPos,
						secondaryColor,
						item.secondaryText.c_str()
					);
				}
			}
			ImGui::PopStyleVar();
		}
		ImGui::End();
		ImGui::PopStyleVar();
	}

	void ConsoleUI::MoveSuggestionSelection(const int delta) {
		auto& state = gConsoleUIData.suggestion;
		if (state.items.empty()) {
			state.selectedIndex = -1;
			return;
		}

		const int count = static_cast<int>(state.items.size());
		if (state.selectedIndex < 0 || state.selectedIndex >= count) {
			state.selectedIndex = 0;
			return;
		}

		int next = (state.selectedIndex + delta) % count;
		if (next < 0) {
			next += count;
		}
		state.selectedIndex = next;
	}

	void ConsoleUI::ApplySuggestionToInputBuffer(
		const std::string& suggestion, const bool appendSpace
	) {
		std::string inputText(mInputBuffer);
		const auto  ctx = ParseSuggestionContext(inputText);
		if (ctx.tokenStart > inputText.size() || ctx.tokenEnd > inputText.
		    size()) {
			return;
		}

		inputText.replace(
			ctx.tokenStart,
			ctx.tokenEnd - ctx.tokenStart,
			suggestion
		);
		const size_t insertPos = ctx.tokenStart + suggestion.size();
		if (appendSpace) {
			const bool hasNextChar       = insertPos < inputText.size();
			const bool hasWhitespaceNext =
				hasNextChar &&
				std::isspace(
					static_cast<unsigned char>(inputText[insertPos])
				) != 0;
			const bool hasSemicolonNext =
				hasNextChar && inputText[insertPos] == ';';
			if (!hasWhitespaceNext && !hasSemicolonNext) {
				inputText.insert(insertPos, 1, ' ');
			}
		}

		const size_t copySize = std::min(
			inputText.size(),
			static_cast<size_t>(IM_ARRAYSIZE(mInputBuffer) - 1)
		);
		std::copy_n(inputText.c_str(), copySize, mInputBuffer);
		mInputBuffer[copySize] = '\0';
	}

	void ConsoleUI::ApplySuggestionToCallbackBuffer(
		ImGuiInputTextCallbackData* data,
		const std::string&          suggestion,
		const bool                  appendSpace
	) {
		const std::string_view input(
			data->Buf, static_cast<size_t>(data->BufTextLen)
		);
		const auto ctx = ParseSuggestionContext(input);
		if (ctx.tokenStart > input.size() || ctx.tokenEnd > input.size()) {
			return;
		}

		const int tokenStart = static_cast<int>(ctx.tokenStart);
		const int tokenLen   = static_cast<int>(ctx.tokenEnd - ctx.tokenStart);
		data->DeleteChars(tokenStart, tokenLen);
		data->InsertChars(tokenStart, suggestion.c_str());
		int newCursor = tokenStart + static_cast<int>(suggestion.size());
		if (
			appendSpace &&
			(newCursor >= data->BufTextLen || (
				 std::isspace(
					 static_cast<unsigned char>(data->Buf[newCursor])
				 ) == 0 &&
				 data->Buf[newCursor] != ';'
			 ))
		) {
			data->InsertChars(newCursor, " ");
			++newCursor;
		}
		data->CursorPos      = newCursor;
		data->SelectionStart = newCursor;
		data->SelectionEnd   = newCursor;
	}

	ConsoleUI::SuggestionContext ConsoleUI::ParseSuggestionContext(
		const std::string_view input
	) {
		SuggestionContext ctx{};

		bool inQuotes = false;
		for (size_t i = 0; i < input.size(); ++i) {
			if (input[i] == '"') {
				inQuotes = !inQuotes;
				continue;
			}

			if (input[i] == ';' && !inQuotes) {
				ctx.segmentStart = i + 1;
			}
		}

		size_t tokenStart = ctx.segmentStart;
		while (
			tokenStart < input.size() &&
			std::isspace(static_cast<unsigned char>(input[tokenStart])) != 0
		) {
			++tokenStart;
		}
		ctx.tokenStart = tokenStart;

		size_t tokenEnd = tokenStart;
		while (
			tokenEnd < input.size() &&
			std::isspace(static_cast<unsigned char>(input[tokenEnd])) == 0 &&
			input[tokenEnd] != ';'
		) {
			++tokenEnd;
		}
		ctx.tokenEnd = tokenEnd;

		if (ctx.tokenEnd > ctx.tokenStart) {
			ctx.token = std::string(
				input.substr(ctx.tokenStart, ctx.tokenEnd - ctx.tokenStart)
			);
			if (
				ctx.tokenEnd < input.size() &&
				std::isspace(
					static_cast<unsigned char>(input[ctx.tokenEnd])
				) != 0
			) {
				ctx.tokenConfirmed = true;
			}
		}

		return ctx;
	}

	std::vector<ConsoleUI::SuggestionItem> ConsoleUI::BuildSuggestionsForToken(
		const std::string_view token
	) const {
		std::vector<SuggestionItem> results;
		std::unordered_set<std::string> seen;
		constexpr size_t fetchCount = kMaxSuggestionDisplayCount * 2;

		auto AppendUnique = [&](
			const std::string& name,
			const bool         isConVar
		) {
			if (name.empty() || seen.contains(name)) {
				return;
			}

			SuggestionItem item;
			item.name     = name;
			item.isConVar = isConVar;
			if (isConVar) {
				item.secondaryText =
					"= " + mConsoleSystem->GetConVarValueString(
						name
					);
			} else {
				if (const auto* cmd = mConsoleSystem->GetConCommand(name)) {
					item.secondaryText = std::string(cmd->GetDescription());
				}
				if (item.secondaryText.empty()) {
					item.secondaryText = "[command]";
				}
			}

			seen.insert(name);
			results.push_back(std::move(item));
		};

		const std::string tokenText(token);
		const std::string lowerToken    = ToLowerAscii(tokenText);
		const auto        varCandidates =
			mConsoleSystem->FindSimilarConVars(token, fetchCount);
		const auto commandCandidates =
			mConsoleSystem->FindSimilarConCommands(token, fetchCount);

		auto IsExactMatch = [&](const std::string& name) {
			return !tokenText.empty() && ToLowerAscii(name) == lowerToken;
		};
		auto IsPrefixMatch = [&](const std::string& name) {
			return !tokenText.empty() &&
			       ToLowerAscii(name).starts_with(lowerToken);
		};

		const bool hasExactVar = std::ranges::any_of(
			varCandidates, IsExactMatch
		);
		const bool hasExactCmd = std::ranges::any_of(
			commandCandidates, IsExactMatch
		);
		const bool hasExact = hasExactVar || hasExactCmd;

		const bool hasPrefixVar = std::ranges::any_of(
			varCandidates,
			IsPrefixMatch
		);
		const bool hasPrefixCmd = std::ranges::any_of(
			commandCandidates,
			IsPrefixMatch
		);
		const bool hasPrefix = hasPrefixVar || hasPrefixCmd;

		auto AppendFiltered = [&](
			const std::vector<std::string>& names,
			const bool                      isConVar
		) {
			for (const auto& name : names) {
				if (hasExact && !IsExactMatch(name)) {
					continue;
				}
				if (!hasExact && hasPrefix && !IsPrefixMatch(name)) {
					continue;
				}
				AppendUnique(name, isConVar);
			}
		};

		AppendFiltered(varCandidates, true);
		AppendFiltered(commandCandidates, false);

		if (results.size() > static_cast<size_t>(kMaxSuggestionDisplayCount)) {
			results.resize(kMaxSuggestionDisplayCount);
		}
		return results;
	}

	/// @brief インプットテキストからのコールバック
	int ConsoleUI::InputTextCallback(ImGuiInputTextCallbackData* data) {
		auto&       c    = gConsoleUIData;
		const auto* self = static_cast<ConsoleUI*>(data->UserData);
		if (!self) {
			return 0;
		}

		auto EnsureSuggestions = [&] {
			if (!gConsoleUIData.suggestion.items.empty()) {
				return true;
			}
			self->UpdateSuggestions(
				std::string_view(
					data->Buf,
					static_cast<size_t>(std::max(data->BufTextLen, 0))
				)
			);
			return !gConsoleUIData.suggestion.items.empty() && data->BufTextLen
			       > 0;
		};

		switch (data->EventFlag) {
			case ImGuiInputTextFlags_CallbackCompletion: {
				if (!EnsureSuggestions()) {
					break;
				}

				auto& suggestionState = gConsoleUIData.suggestion;
				if (suggestionState.items.empty()) {
					break;
				}

				suggestionState.selectedIndex = std::max(
					suggestionState.selectedIndex, 0
				);

				const bool reverse = ImGui::GetIO().KeyShift;
				MoveSuggestionSelection(reverse ? -1 : 1);
				if (
					suggestionState.selectedIndex >= 0 &&
					std::cmp_less(
						suggestionState.selectedIndex,
						suggestionState.items.size()
					)
				) {
					const auto& selected =
						suggestionState.items[static_cast<size_t>(
							suggestionState.selectedIndex
						)];
					c.suppressSuggestionRefreshOnEdit = true;
					ApplySuggestionToCallbackBuffer(data, selected.name, false);
				}
			}
			break;

			case ImGuiInputTextFlags_CallbackHistory: {
				if (EnsureSuggestions()) {
					if (data->EventKey == ImGuiKey_UpArrow) {
						MoveSuggestionSelection(1);
						const auto& selected =
							c.suggestion.items[static_cast<size_t>(
								c.suggestion.selectedIndex
							)];
						c.suppressSuggestionRefreshOnEdit = true;
						ApplySuggestionToCallbackBuffer(
							data, selected.name, false
						);
						break;
					}
					if (data->EventKey == ImGuiKey_DownArrow) {
						MoveSuggestionSelection(-1);
						const auto& selected =
							c.suggestion.items[static_cast<size_t>(
								c.suggestion.selectedIndex
							)];
						c.suppressSuggestionRefreshOnEdit = true;
						ApplySuggestionToCallbackBuffer(
							data, selected.name, false
						);
						break;
					}
				}

				const int historySize = static_cast<int>(c.history.Size());
				if (historySize <= 0) {
					break;
				}

				// ↑↓を押したら入力中だったテキストを scratch に保存
				if (!c.browsing) {
					c.scratch.assign(data->Buf, data->BufTextLen);
					c.browsing     = true;
					c.historyIndex = historySize; // 末尾（scratch位置）から開始
				}

				const int prevHistoryIndex = c.historyIndex;
				if (data->EventKey == ImGuiKey_UpArrow) {
					if (c.historyIndex > 0) {
						c.historyIndex--;
					}
				} else if (data->EventKey == ImGuiKey_DownArrow) {
					if (c.historyIndex < historySize) {
						c.historyIndex++;
					} else {
						c.historyIndex = historySize;
					}
				}

				if (prevHistoryIndex != c.historyIndex) {
					data->DeleteChars(0, data->BufTextLen);
					if (c.historyIndex < historySize) {
						data->InsertChars(
							0, c.history[static_cast<size_t>(c.historyIndex)].
							c_str()
						);
					} else {
						// 末尾に戻ったら 入力中テキストを復元
						data->InsertChars(0, c.scratch.c_str());
					}
				}
				break;
			}

			case ImGuiInputTextFlags_CallbackEdit:
				// 編集が入ったら「履歴閲覧中」フラグを解除（次の↑↓で scratch を取り直す）
				c.browsing = false;
				if (c.suppressSuggestionRefreshOnEdit) {
					c.suppressSuggestionRefreshOnEdit = false;
					break;
				}
				self->UpdateSuggestions(
					std::string_view(
						data->Buf,
						static_cast<size_t>(std::max(data->BufTextLen, 0))
					)
				);
				break;

			case ImGuiInputTextFlags_CallbackResize: {
			}
			break;
			default: ;
		}
		return 0;
	}

	size_t ConsoleUI::FilteredToActualIndex(const int filteredIndex) {
		if (filteredIndex < 0) {
			return SIZE_MAX;
		}
		const auto idx = static_cast<size_t>(filteredIndex);
		if (idx >= gConsoleUIData.filteredToActual.size()) {
			return SIZE_MAX;
		}
		return gConsoleUIData.filteredToActual[idx];
	}

	void ConsoleUI::DrawLogTable(const ImVec2& childSize) {
		if (!ImGui::BeginTable(
			"Show##ConsoleUI", 2,
			ImGuiTableFlags_ScrollX |
			ImGuiTableFlags_ScrollY |
			ImGuiTableFlags_SizingFixedFit |
			ImGuiTableFlags_BordersV |
			ImGuiTableFlags_NoHostExtendX,
			childSize
		)) {
			return;
		}

		ImGui::TableSetupColumn("Log", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Channel", ImGuiTableColumnFlags_WidthFixed);
		ImGui::TableHeadersRow();

		int  visibleIndex           = 0;
		bool requestOpenContextMenu = false;

		// 選択バッファはログ数に合わせる
		gConsoleUIData.selected.resize(
			mConsoleSystem->GetLogBuffer().Size(), false
		);

		// フィルタ後の表示行 から actualIndex マップを作成
		gConsoleUIData.filteredToActual.clear();
		gConsoleUIData.filteredToActual.reserve(
			mConsoleSystem->GetLogBuffer().Size()
		);
		for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size(); ++i) {
			const auto& buffer = mConsoleSystem->GetLogBuffer()[i];
			if (
				gConsoleUIData.currentFilterChannel == kChannelNone ||
				buffer.channel == gConsoleUIData.currentFilterChannel
			) {
				gConsoleUIData.filteredToActual.push_back(i);
			}
		}

		DrawLogRows(visibleIndex, requestOpenContextMenu);

		if (requestOpenContextMenu) {
			OpenConsoleContextMenuPopup();
		}
		ShowContextMenu();
		HandleCopyShortcut();
		CheckScroll();

		ImGui::EndTable();
	}

	void ConsoleUI::DrawLogRows(
		int& visibleIndex, bool& requestOpenContextMenu
	) const {
		// 複数行ログで可変行高になるため全行を順に描画する
		for (size_t row = 0; row < gConsoleUIData.filteredToActual.size(); ++
		     row) {
			const size_t actualIndex = gConsoleUIData.filteredToActual[row];
			if (DrawLogRow(
				actualIndex,
				static_cast<int>(row),
				requestOpenContextMenu
			)) {
				visibleIndex++;
			}
		}
	}

	bool ConsoleUI::DrawLogRow(
		const size_t actualIndex,
		const int    visibleIndex,
		bool&        requestOpenContextMenu
	) const {
		const auto& buffer = mConsoleSystem->GetLogBuffer()[actualIndex];
		const int   lineCount = CountDisplayLines(buffer.message);
		const float lineHeight = ImGui::GetTextLineHeight();
		const float multiLineHeight = std::max(
			lineHeight * static_cast<float>(lineCount) +
			ImGui::GetStyle().FramePadding.y * 2.0f,
			ImGui::GetFrameHeight()
		);

		if (lineCount > 1) {
			ImGui::TableNextRow(0, multiLineHeight);
		} else {
			ImGui::TableNextRow();
		}
		ImGui::TableSetColumnIndex(0);

		ImGui::PushID(static_cast<int>(actualIndex));

		const bool        isSelected   = gConsoleUIData.selected[actualIndex];
		const std::string selectableId = "##LogRow_" + std::to_string(
			                                 actualIndex
		                                 );
		ImGui::Selectable(
			selectableId.c_str(),
			isSelected,
			ImGuiSelectableFlags_SpanAllColumns |
			ImGuiSelectableFlags_AllowOverlap,
			lineCount > 1 ? ImVec2(0.0f, multiLineHeight) : ImVec2(0.0f, 0.0f)
		);

		const bool isRowVisible = ImGui::IsItemVisible();

		if (isRowVisible) {
			PushTextColor(buffer);
			const auto textColor = ImGui::GetColorU32(ImGuiCol_Text);
			ImGui::PopStyleColor();
			const auto textMin = ImVec2(
				ImGui::GetItemRectMin().x + ImGui::GetStyle().FramePadding.x,
				ImGui::GetItemRectMin().y + ImGui::GetStyle().FramePadding.y
			);
			const ImVec2 textMax = ImGui::GetItemRectMax();
			ImGui::PushClipRect(textMin, textMax, true);
			ImGui::GetWindowDrawList()->AddText(
				textMin,
				textColor,
				buffer.message.c_str()
			);
			ImGui::PopClipRect();
		}

		// 左クリック選択
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
			if (ImGui::GetIO().KeyCtrl) {
				gConsoleUIData.selected[actualIndex] =
					!gConsoleUIData.selected[actualIndex];
				gConsoleUIData.lastSelectedFilteredIndex = visibleIndex;
			} else if (
				ImGui::GetIO().KeyShift &&
				gConsoleUIData.lastSelectedFilteredIndex != -1
			) {
				if (!ImGui::GetIO().KeyCtrl) {
					std::ranges::fill(gConsoleUIData.selected, false);
				}

				const int start = std::min(
					gConsoleUIData.lastSelectedFilteredIndex, visibleIndex
				);
				const int end = std::max(
					gConsoleUIData.lastSelectedFilteredIndex, visibleIndex
				);
				for (int j = start; j <= end; ++j) {
					const size_t idx = FilteredToActualIndex(j);
					if (idx != SIZE_MAX) {
						gConsoleUIData.selected[idx] = true;
					}
				}
			} else {
				std::ranges::fill(gConsoleUIData.selected, false);
				gConsoleUIData.selected[actualIndex]     = true;
				gConsoleUIData.lastSelectedFilteredIndex = visibleIndex;
			}
		}

		// 右クリック: 選択済みなら選択維持、未選択なら単一選択へ
		if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
			if (!gConsoleUIData.selected[actualIndex]) {
				std::ranges::fill(gConsoleUIData.selected, false);
				gConsoleUIData.selected[actualIndex]     = true;
				gConsoleUIData.lastSelectedFilteredIndex = visibleIndex;
			}
			requestOpenContextMenu = true;
		}

		ImGui::PopID();

		// チャンネル列
		ImGui::TableSetColumnIndex(1);
		if (isRowVisible && buffer.channel != kChannelNone) {
			if (ImGui::TextLink(
				(buffer.channel + "##" + std::to_string(actualIndex)).c_str()
			)) {
				if (gConsoleUIData.currentFilterChannel == buffer.channel) {
					gConsoleUIData.currentFilterChannel = kChannelNone;
				} else {
					gConsoleUIData.currentFilterChannel = buffer.channel;
				}
			}
		}

		return true;
	}

	void ConsoleUI::CopySelectedToClipboard() const {
		std::string      copiedText;
		constexpr size_t maxCopyLength = static_cast<size_t>(1024) * 1024;
		// 1MB

		for (size_t i = 0; i < mConsoleSystem->GetLogBuffer().Size(); ++i) {
			if (!gConsoleUIData.selected[i]) {
				continue;
			}

			const auto&  buffer       = mConsoleSystem->GetLogBuffer()[i];
			const size_t requiredSize =
				copiedText.size() + buffer.message.size() + 1;

			if (requiredSize > maxCopyLength) {
				if (!copiedText.empty() && copiedText.back() != '\n') {
					copiedText += '\n';
				}
				copiedText += "... (copy truncated) ...\n";
				break;
			}

			copiedText += buffer.message;
			copiedText += '\n';
		}

		ImGui::SetClipboardText(copiedText.c_str());
	}

	void ConsoleUI::HandleCopyShortcut() const {
		if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C)) {
			CopySelectedToClipboard();
		}
	}

	void ConsoleUI::OpenConsoleContextMenuPopup() {
		ImGui::OpenPopup(kConsoleUIContextPopupId);
	}
}
#endif
