#if defined(UNNAMED_WITH_EDITOR)
#include "ConVarHelper.h"

#include <algorithm>
#include <format>
#include <imgui.h>
#include <utility>
#include <Windows.h>

#include <engine/ImGui/ImGuiWidgets.h>

#include "ConsoleSystem.h"

#include "engine/Properties.h"
#include "engine/ImGui/Icons.h"

namespace Unnamed {
	static constexpr uint32_t kGridMaxWidth  = 25;    // グリッドの最大幅
	static constexpr uint32_t kGridMaxHeight = 50;    // グリッドの最大高
	static constexpr float    kCellHeight    = 24.0f; // グリッドセルの高さ

	void ConVarHelper::Show(bool& showConVarHelper) {
		if (!showConVarHelper) {
			return;
		}

		ImGui::SetNextWindowSizeConstraints(
			{172.0f, 0.0f},
			{0xFFFF, 0xFFFF}
		);

		constexpr ImGuiWindowFlags windowFlags =
			ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_MenuBar;

		const bool bOpen = ImGui::Begin(
			"ConVar Helper",
			&showConVarHelper,
			windowFlags
		);

		if (bOpen) {
			ShowMenuBar();
			ShowPageSelect();
			ShowGridSizeControls();
			ShowGrid();

			if (mShowElementPopup) {
				ShowElementEditPopup();
			}
		}
		ImGui::End();
	}

	void ConVarHelper::ShowMenuBar() {
		// メニューバー
		if (ImGui::BeginMenuBar()) {
			ImGui::PushStyleVar(
				ImGuiStyleVar_WindowPadding,
				ImVec2(kPopupPadding, kPopupPadding)
			);

			if (ImGui::BeginMenu("File")) {
				if (ImGuiWidgets::MenuItemWithIcon(
					"Import Page", kIconDownload
				)) {
					ImportPage();
				}

				if (ImGuiWidgets::MenuItemWithIcon(
					"Export Page", kIconUpload
				)) {
					ExportPage();
				}
				ImGui::EndMenu();
			}

			ImGui::PopStyleVar();

			ImGui::EndMenuBar();
		}
	}

	void ConVarHelper::ShowPageSelect() {
		// ラベルを左側に配置したいだけなのにぃ
		const auto   label     = "Page";
		const ImVec2 labelSize = ImGui::CalcTextSize(label);
		ImVec2       labelPos  = ImGui::GetCursorPos();
		ImGui::SetCursorPosX(labelSize.x + ImGui::GetStyle().ItemSpacing.x);

		// ComboBoxの表示
		{
			if (
				ImGui::BeginCombo(
					"##Page",
					mPages[mSelectedPageIndex].name.c_str(),
					ImGuiComboFlags_WidthFitPreview
				)
			) {
				// ページ一覧を表示
				for (uint32_t n = 0; n < mPages.size(); n++) {
					const bool isSelected =
						std::cmp_equal(mSelectedPageIndex, n);
					if (ImGui::Selectable(mPages[n].name.c_str(), isSelected)) {
						mSelectedPageIndex = n;
					}
					if (isSelected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			// マウスホイールスクロールの処理
			ImGuiWidgets::HandleHoveredComboMenuMouseWheelScroll(
				mSelectedPageIndex,
				static_cast<uint32_t>(mPages.size())
			);
		}

		// ラベルの垂直位置を中央に調整
		labelPos.y += ImGui::GetItemRectSize().y * 0.5f - labelSize.y * 0.5f;

		// 位置を元に戻してラベルを描画
		ImGui::SetCursorPos(labelPos);
		ImGui::TextUnformatted(label);
	}

	void ConVarHelper::ShowGridSizeControls() {
		// ラベルを左側に配置したいだけなのにぃ
		const auto   label     = "Grid";
		const ImVec2 labelSize = ImGui::CalcTextSize(label);
		ImVec2       labelPos  = ImGui::GetCursorPos();
		ImGui::SetCursorPosX(labelSize.x + ImGui::GetStyle().ItemSpacing.x);

		const float windowWidth = ImGui::GetContentRegionAvail().x;

		const float xTextWidth = ImGui::CalcTextSize("x").x;
		const float spacing    = ImGui::GetStyle().ItemSpacing.x;
		const float inputWidth = (windowWidth - labelSize.x - xTextWidth -
		                          spacing * 4) * 0.5f;

		ImGui::SetNextItemWidth(inputWidth);
		int tmpWidth = static_cast<int>(
			mPages[mSelectedPageIndex].grid.width
		);
		if (ImGui::InputInt("##Width", &tmpWidth, 1)) {
			if (tmpWidth > 0 &&
			    !std::cmp_equal(
				    tmpWidth,
				    static_cast<int>(mPages[mSelectedPageIndex].grid.width)
			    )) {
				tmpWidth = std::clamp(
					tmpWidth, 1, static_cast<int>(kGridMaxWidth)
				);
				RearrangeGridElements(
					tmpWidth,
					mPages[mSelectedPageIndex].grid.height
				);
				mPages[mSelectedPageIndex].grid.width = tmpWidth;
			}
		}

		ImGui::SameLine();
		ImGui::TextUnformatted("x");
		ImGui::SameLine();

		ImGui::SetNextItemWidth(inputWidth);
		int tmpHeight = static_cast<int>(
			mPages[mSelectedPageIndex].grid.height
		);
		if (ImGui::InputInt("##Height", &tmpHeight, 1)) {
			if (tmpHeight > 0 &&
			    !std::cmp_equal(
				    tmpHeight,
				    static_cast<int>(mPages[mSelectedPageIndex].grid.height)
			    )) {
				tmpHeight = std::clamp(
					tmpHeight, 1, static_cast<int>(kGridMaxHeight)
				);
				RearrangeGridElements(
					mPages[mSelectedPageIndex].grid.width,
					tmpHeight
				);
				mPages[mSelectedPageIndex].grid.height = tmpHeight;
			}
		}

		// ラベルの垂直位置を中央に調整
		labelPos.y += ImGui::GetItemRectSize().y * 0.5f - labelSize.y * 0.5f;

		// 位置を元に戻してラベルを描画
		ImGui::SetCursorPos(labelPos);
		ImGui::TextUnformatted(label);
	}

	/// @brief Vec4 を ImVec4 に変換します
	/// @param v 変換する Vec4
	/// @return 変換後の ImVec4
	static ImVec4 ToImVec4Local(const Vec4& v) {
		return {v.x, v.y, v.z, v.w};
	}

	/// @brief ImVec4 を Vec4 に変換します
	/// @param v 変換する ImVec4
	/// @return 変換後の Vec4
	static Vec4 ToVec4Local(const ImVec4& v) {
		return {v.x, v.y, v.z, v.w};
	}

	void ConVarHelper::ShowGrid() {
		const auto& grid = mPages[mSelectedPageIndex].grid;

		// グリッド要素の数を正しく保つ
		const size_t expectedSize =
			static_cast<size_t>(grid.width) * grid.height;
		auto& elements = mPages[mSelectedPageIndex].elements;
		if (elements.size() != expectedSize) {
			elements.resize(expectedSize);
		}

		if (
			ImGui::BeginTable(
				"GridTable", static_cast<int>(grid.width),
				ImGuiTableFlags_Borders |
				ImGuiTableFlags_SizingFixedFit |
				ImGuiTableFlags_NoPadInnerX |
				ImGuiTableFlags_NoPadOuterX
			)
		) {
			const float columnWidth =
				ImGui::GetContentRegionAvail().x /
				static_cast<float>(grid.width);

			for (uint32_t i = 0; i < grid.width; ++i) {
				ImGui::TableSetupColumn(
					"", ImGuiTableColumnFlags_WidthFixed, columnWidth
				);
			}

			for (uint32_t row = 0; row < grid.height; ++row) {
				ImGui::TableNextRow(0, kCellHeight);
				for (uint32_t col = 0; col < grid.width; ++col) {
					ImGui::TableSetColumnIndex(static_cast<int>(col));

					// セルのインデックスを計算
					const size_t cellIndex = row * grid.width + col;
					GridElement& element   = elements[cellIndex];

					// IDを設定
					std::string cellId = std::format(
						"Cell ({}, {})", col, row
					);

					// ボタンのサイズをセルの幅と高さに合わせる
					auto cellSize = ImVec2(columnWidth, kCellHeight);

					switch (element.element.index()) {
						// 空
						case 0: {
							ImGui::InvisibleButton(cellId.c_str(), cellSize);

							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								ImGui::Spacing();
								if (ImGuiWidgets::MenuItemWithIcon(
									"Add Element", kIconRectangle, nullptr,
									mShowElementPopup
								)) {
									mShowElementPopup       = true;
									mEditingElementIndex    = cellIndex;
									mElementEditInitialized = false;
								}
								ImGui::Spacing();
								ImGui::EndPopup();
							}
							break;
						}
						// ラベル
						case 1: {
							// ラベルの場合は、テキストを表示
							const auto& labelElement =
								std::get<Label>(element.element);
							ImGui::PushStyleColor(
								ImGuiCol_Button,
								ImVec4(
									labelElement.bgColor.x,
									labelElement.bgColor.y,
									labelElement.bgColor.z,
									labelElement.bgColor.w
								)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonActive,
								ImVec4(
									labelElement.bgColor.x,
									labelElement.bgColor.y,
									labelElement.bgColor.z,
									labelElement.bgColor.w
								)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonHovered,
								ImVec4(
									labelElement.bgColor.x,
									labelElement.bgColor.y,
									labelElement.bgColor.z,
									labelElement.bgColor.w
								)
							);
							ImGui::PushStyleColor(
								ImGuiCol_Text,
								ImVec4(
									labelElement.fgColor.x,
									labelElement.fgColor.y,
									labelElement.fgColor.z,
									labelElement.fgColor.w
								)
							);
							ImGui::Button(
								(labelElement.label + "##" + cellId).c_str(),
								cellSize
							);
							ImGui::PopStyleColor(4);
							// 右クリックで編集/削除
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								ImGui::Spacing();
								if (ImGuiWidgets::MenuItemWithIcon(
									"Edit", kIconSettings
								)) {
									mShowElementPopup       = true;
									mEditingElementIndex    = cellIndex;
									mElementEditInitialized = false;
								}
								if (ImGuiWidgets::MenuItemWithIcon(
									"Delete", kIconClose
								)) {
									element.element = Empty{};
								}
								ImGui::Spacing();
								ImGui::EndPopup();
							}
							break;
						}

						case 2: {
							// Button 表示
							const auto& btn = std::get<Button>(element.element);
							ImGui::PushStyleColor(
								ImGuiCol_Button, ToImVec4Local(btn.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonActive,
								ToImVec4Local(btn.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonHovered,
								ToImVec4Local(btn.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_Text, ToImVec4Local(btn.fgColor)
							);
							if (
								ImGui::Button(
									(btn.label + "##" + cellId).c_str(),
									cellSize
								)
							) {
								if (mConsoleSystem) {
									// コマンドをコンソールで実行
									mConsoleSystem->ExecuteCommand(
										btn.command,
										EXEC_FLAG::FROM_CONSOLE
									);
								}
							}
							ImGui::PopStyleColor(4);
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								ImGui::Spacing();
								if (ImGuiWidgets::MenuItemWithIcon(
									"Edit", kIconSettings
								)) {
									mShowElementPopup       = true;
									mEditingElementIndex    = cellIndex;
									mElementEditInitialized = false;
								}
								if (ImGuiWidgets::MenuItemWithIcon(
									"Delete", kIconClose
								)) {
									element.element = Empty{};
								}
								ImGui::Spacing();
								ImGui::EndPopup();
							}
							break;
						}
						case 3: {
							// ExecutableButton 表示
							const auto& eb = std::get<ExecutableButton>(
								element.element
							);
							ImGui::PushStyleColor(
								ImGuiCol_Button, ToImVec4Local(eb.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonActive, ToImVec4Local(eb.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_ButtonHovered,
								ToImVec4Local(eb.bgColor)
							);
							ImGui::PushStyleColor(
								ImGuiCol_Text, ToImVec4Local(eb.fgColor)
							);
							if (
								ImGui::Button(
									(eb.label + "##" + cellId).c_str(), cellSize
								)
							) {
								// TODO: ターミナルでコマンド実行
							}
							ImGui::PopStyleColor(4);
							if (ImGui::BeginPopupContextItem(cellId.c_str())) {
								ImGui::Spacing();
								if (ImGuiWidgets::MenuItemWithIcon(
									"Edit", kIconSettings
								)) {
									mShowElementPopup       = true;
									mEditingElementIndex    = cellIndex;
									mElementEditInitialized = false;
								}
								if (ImGuiWidgets::MenuItemWithIcon(
									"Delete", kIconClose
								)) {
									element.element = Empty{};
								}
								ImGui::Spacing();
								ImGui::EndPopup();
							}
							break;
						}
						default: ;
					}
				}
			}

			ImGui::EndTable();
		}
	}

	/// @brief std::string 用の InputText ラッパー
	/// @param label ラベル
	/// @param value 編集する std::string
	/// @param flags ImGuiInputTextFlags
	/// @return 変更があった場合に true を返す
	static bool InputTextStdString(
		const char*               label, std::string& value,
		const ImGuiInputTextFlags flags = 0
	) {
		// imgui_stdlib を使っていない前提の簡易ラッパ
		// 最低限の容量を確保し、NUL 終端を保証しつつ編集後に実長へ戻す。
		constexpr size_t  kMinCapacity = 256;
		const size_t      capacity = std::max(value.size() + 1, kMinCapacity);
		std::vector<char> buf(capacity, '\0');
		if (!value.empty()) {
			const size_t copyLen = std::min(value.size(), capacity - 1);
			memcpy(buf.data(), value.data(), copyLen);
			buf[copyLen] = '\0';
		}
		const bool changed = ImGui::InputText(
			label, buf.data(), buf.size(), flags
		);
		if (changed) {
			value = buf.data();
		}
		return changed;
	}

	void ConVarHelper::ShowElementEditPopup() {
		if (!mShowElementPopup) {
			mElementEditInitialized = false;
			return;
		}

		auto& target = mPages[mSelectedPageIndex].elements[
			mEditingElementIndex];

		// 初回だけスナップショットを取り、作業中のコピーを作る
		if (!mElementEditInitialized) {
			mElementEditOriginal    = target;
			mElementEditWorking     = target;
			mElementEditInitialized = true;

			// ExecutableButton は他の全部を持ってるので、共通項をここで出しとく
			mElementEditDraftExecutableButton = ExecutableButton{};
			mElementEditDraftButtonCommand.clear();

			switch (static_cast<int>(target.element.index())) {
				case 1: {
					const auto& l = std::get<Label>(target.element);
					mElementEditDraftExecutableButton.label = l.label;
					mElementEditDraftExecutableButton.bgColor = l.bgColor;
					mElementEditDraftExecutableButton.fgColor = l.fgColor;
					mElementEditDraftExecutableButton.description = l.
						description;
					break;
				}
				case 2: {
					const auto& b = std::get<Button>(target.element);
					mElementEditDraftExecutableButton.label = b.label;
					mElementEditDraftExecutableButton.bgColor = b.bgColor;
					mElementEditDraftExecutableButton.fgColor = b.fgColor;
					mElementEditDraftExecutableButton.description = b.
						description;
					mElementEditDraftExecutableButton.command = b.command;
					mElementEditDraftButtonCommand            = b.command;
					break;
				}
				case 3: {
					mElementEditDraftExecutableButton = std::get<
						ExecutableButton>(target.element);
					break;
				}
				default: break;
			}

			ImGui::OpenPopup("Edit Element");
		}

		if (ImGui::BeginPopupModal(
			"Edit Element", &mShowElementPopup,
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_AlwaysAutoResize
		)) {
			int typeIndex = static_cast<int>(mElementEditWorking.element.
				index());
			const int prevType = typeIndex;

			ImGui::TextUnformatted("Element Type:");
			ImGui::SameLine();
			ImGui::RadioButton("Empty", &typeIndex, 0);
			ImGui::SameLine();
			ImGui::RadioButton("Label", &typeIndex, 1);
			ImGui::SameLine();
			ImGui::RadioButton("Button", &typeIndex, 2);
			ImGui::SameLine();
			ImGui::RadioButton("ExecutableButton", &typeIndex, 3);

			if (typeIndex != prevType) {
				// 切り替え直前の内容を、基底ドラフトへ退避
				switch (prevType) {
					case 1: {
						const auto& l = std::get<Label>(
							mElementEditWorking.element
						);
						mElementEditDraftExecutableButton.label = l.label;
						mElementEditDraftExecutableButton.bgColor = l.bgColor;
						mElementEditDraftExecutableButton.fgColor = l.fgColor;
						mElementEditDraftExecutableButton.description = l.
							description;
						break;
					}
					case 2: {
						const auto& b = std::get<Button>(
							mElementEditWorking.element
						);
						mElementEditDraftExecutableButton.label = b.label;
						mElementEditDraftExecutableButton.bgColor = b.bgColor;
						mElementEditDraftExecutableButton.fgColor = b.fgColor;
						mElementEditDraftExecutableButton.description = b.
							description;
						mElementEditDraftExecutableButton.command = b.command;
						mElementEditDraftButtonCommand            = b.command;
						break;
					}
					case 3: {
						mElementEditDraftExecutableButton = std::get<
							ExecutableButton>(mElementEditWorking.element);
						break;
					}
					default: break;
				}

				// 切り替え先の要素を基底データから生成（共通項は同じ値を参照）
				switch (typeIndex) {
					case 0: mElementEditWorking.element = Empty{};
						break;
					case 1: mElementEditWorking.element = Label{
						        .label = mElementEditDraftExecutableButton.
						        label,
						        .bgColor = mElementEditDraftExecutableButton.
						        bgColor,
						        .fgColor = mElementEditDraftExecutableButton.
						        fgColor,
						        .description = mElementEditDraftExecutableButton
						        .description
					        };
						break;
					case 2: mElementEditWorking.element = Button{
						        .label = mElementEditDraftExecutableButton.
						        label,
						        .command =
						        mElementEditDraftButtonCommand.empty() ?
							        mElementEditDraftExecutableButton.command :
							        mElementEditDraftButtonCommand,
						        .bgColor = mElementEditDraftExecutableButton.
						        bgColor,
						        .fgColor = mElementEditDraftExecutableButton.
						        fgColor,
						        .description = mElementEditDraftExecutableButton
						        .description
					        };
						break;
					case 3: mElementEditWorking.element =
					        mElementEditDraftExecutableButton;
						break;
					default: break;
				}
			}

			ImGui::Separator();

			// 型別の編集 UI
			switch (static_cast<int>(mElementEditWorking.element.index())) {
				case 0: ImGui::TextUnformatted("(Empty)");
					break;
				case 1: LabelEdit();
					break;
				case 2: ButtonEdit();
					break;
				case 3: ExecutableButtonEdit();
					break;
				default: break;
			}

			ImGui::Separator();

			// Accept / Cancel（水平中央）
			const auto  acceptLabel = "Accept";
			const auto  cancelLabel = "Cancel";
			const float acceptW     =
				ImGui::CalcTextSize(acceptLabel).x + ImGui::GetStyle().
				FramePadding.x * 2.0f;
			const float cancelW =
				ImGui::CalcTextSize(cancelLabel).x + ImGui::GetStyle().
				FramePadding.x * 2.0f;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float totalW  = acceptW + spacing + cancelW;
			const float availW  = ImGui::GetContentRegionAvail().x;
			if (availW > totalW) {
				ImGui::SetCursorPosX(
					ImGui::GetCursorPosX() + (availW - totalW) * 0.5f
				);
			}

			if (ImGui::Button(acceptLabel)) {
				target                  = mElementEditWorking;
				mShowElementPopup       = false;
				mElementEditInitialized = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button(cancelLabel)) {
				target                  = mElementEditOriginal;
				mShowElementPopup       = false;
				mElementEditInitialized = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}

	void ConVarHelper::LabelEdit() {
		auto& elem = std::get<Label>(mElementEditWorking.element);

		// 編集
		(void)InputTextStdString("Label##LabelText", elem.label);

		ImVec4 bg = ToImVec4Local(elem.bgColor);
		ImVec4 fg = ToImVec4Local(elem.fgColor);
		if (ImGui::ColorEdit4(
			"BG##LabelBG", &bg.x, ImGuiColorEditFlags_NoInputs
		)) {
			elem.bgColor = ToVec4Local(bg);
		}
		if (ImGui::ColorEdit4(
			"FG##LabelFG", &fg.x, ImGuiColorEditFlags_NoInputs
		)) {
			elem.fgColor = ToVec4Local(fg);
		}

		// 説明はフラグ無し
		InputTextStdString(
			"Description##LabelDesc", elem.description
		);

		// 共通項を基底へ同期
		mElementEditDraftExecutableButton.label       = elem.label;
		mElementEditDraftExecutableButton.bgColor     = elem.bgColor;
		mElementEditDraftExecutableButton.fgColor     = elem.fgColor;
		mElementEditDraftExecutableButton.description = elem.description;
	}

	void ConVarHelper::ButtonEdit() {
		auto& elem = std::get<Button>(mElementEditWorking.element);

		(void)InputTextStdString("Label##BtnLabel", elem.label);
		(void)InputTextStdString("Command##BtnCmd", elem.command);

		ImVec4 bg = ToImVec4Local(elem.bgColor);
		ImVec4 fg = ToImVec4Local(elem.fgColor);
		if (ImGui::ColorEdit4(
			"BG##BtnBG", &bg.x, ImGuiColorEditFlags_NoInputs
		)) {
			elem.bgColor = ToVec4Local(bg);
		}
		if (ImGui::ColorEdit4(
			"FG##BtnFG", &fg.x, ImGuiColorEditFlags_NoInputs
		)) {
			elem.fgColor = ToVec4Local(fg);
		}

		// 説明
		InputTextStdString(
			"Description##BtnDesc", elem.description
		);

		// 共通項を基底へ同期
		mElementEditDraftExecutableButton.label       = elem.label;
		mElementEditDraftExecutableButton.bgColor     = elem.bgColor;
		mElementEditDraftExecutableButton.fgColor     = elem.fgColor;
		mElementEditDraftExecutableButton.description = elem.description;
		// Button固有
		mElementEditDraftExecutableButton.command = elem.command;
		mElementEditDraftButtonCommand.clear();
		(void)mElementEditDraftButtonCommand.append(elem.command);
	}

	void ConVarHelper::ExecutableButtonEdit() {
		auto& elem = std::get<ExecutableButton>(mElementEditWorking.element);

		(void)InputTextStdString("Label##ExeLabel", elem.label);
		(void)InputTextStdString("Command##ExeCmd", elem.command);
		(void)InputTextStdString("Arguments##ExeArgs", elem.arguments);

		ImVec4 bg = ToImVec4Local(elem.bgColor);
		ImVec4 fg = ToImVec4Local(elem.fgColor);
		if (ImGui::ColorEdit4(
			"BG##ExeBG", &bg.x, ImGuiColorEditFlags_NoInputs
		)) {
			elem.bgColor = ToVec4Local(bg);
		}
		if (ImGui::ColorEdit4(
			"FG##ExeFG", &fg.x, ImGuiColorEditFlags_NoInputs
		)) {
			elem.fgColor = ToVec4Local(fg);
		}

		(void)InputTextStdString(
			"Description##ExeDesc", elem.description,
			ImGuiInputTextFlags_AutoSelectAll
		);

		// そのまま基底へ同期
		mElementEditDraftExecutableButton = elem;
	}

	void ConVarHelper::ImportPage() {
	}

	void ConVarHelper::ExportPage() {
	}

	void ConVarHelper::RearrangeGridElements(
		const uint32_t newWidth, const uint32_t newHeight
	) {
		newWidth;
		newHeight;
	}
}
#endif
