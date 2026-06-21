#include "pch.h"

#ifdef _DEBUG

#include <engine/ImGui/ImGuiUtil.h>

#include "Icons.h"

#include "core/string/StrUtil.h"

namespace ImGuiUtil {
	/// @brief Vec4型をImVec4型に変換します。
	/// @param vec 変換するVec4型のベクトル
	/// @return 変換後のImVec4型のベクトル
	ImVec4 ToImVec4(const Vec4& vec) {
		return {vec.x, vec.y, vec.z, vec.w};
	}

	/// @brief ImGuiのダークテーマスタイルを設定します。
	void StyleColorsDark() {
		ImGuiStyle& style  = ImGui::GetStyle();
		ImVec4*     colors = style.Colors;

		colors[ImGuiCol_ChildBg]               = {0.078f, 0.078f, 0.078f, 1.0f};
		colors[ImGuiCol_PopupBg]               = {0.078f, 0.078f, 0.078f, 1.0f};
		colors[ImGuiCol_WindowBg]              = {0.14f, 0.14f, 0.14f, 0.94f};
		colors[ImGuiCol_Border]                = {0.10f, 0.10f, 0.10f, 1.00f};
		colors[ImGuiCol_FrameBg]               = {0.18f, 0.18f, 0.18f, 1.00f};
		colors[ImGuiCol_FrameBgHovered]        = {0.24f, 0.24f, 0.24f, 1.00f};
		colors[ImGuiCol_FrameBgActive]         = {0.26f, 0.26f, 0.26f, 1.00f};
		colors[ImGuiCol_TitleBg]               = {0.14f, 0.14f, 0.14f, 1.00f};
		colors[ImGuiCol_TitleBgActive]         = {0.18f, 0.18f, 0.18f, 1.00f};
		colors[ImGuiCol_TitleBgCollapsed]      = {0.14f, 0.14f, 0.14f, 1.00f};
		colors[ImGuiCol_Button]                = {0.20f, 0.20f, 0.20f, 1.00f};
		colors[ImGuiCol_ButtonHovered]         = {0.30f, 0.30f, 0.30f, 1.00f};
		colors[ImGuiCol_ButtonActive]          = {0.35f, 0.35f, 0.35f, 1.00f};
		colors[ImGuiCol_Header]                = {0.23f, 0.23f, 0.23f, 1.00f};
		colors[ImGuiCol_HeaderHovered]         = {0.28f, 0.28f, 0.28f, 1.00f};
		colors[ImGuiCol_HeaderActive]          = {0.32f, 0.32f, 0.32f, 1.00f};
		colors[ImGuiCol_ResizeGrip]            = {0.30f, 0.30f, 0.30f, 1.00f};
		colors[ImGuiCol_ResizeGripHovered]     = {0.45f, 0.45f, 0.45f, 1.00f};
		colors[ImGuiCol_ResizeGripActive]      = {0.55f, 0.55f, 0.55f, 1.00f};
		colors[ImGuiCol_ScrollbarBg]           = {0.15f, 0.15f, 0.15f, 1.00f};
		colors[ImGuiCol_ScrollbarGrab]         = {0.30f, 0.30f, 0.30f, 1.00f};
		colors[ImGuiCol_ScrollbarGrabHovered]  = {0.35f, 0.35f, 0.35f, 1.00f};
		colors[ImGuiCol_ScrollbarGrabActive]   = {0.40f, 0.40f, 0.40f, 1.00f};
		colors[ImGuiCol_Text]                  = {0.71f, 0.71f, 0.71f, 1.00f};
		colors[ImGuiCol_TextDisabled]          = {0.50f, 0.50f, 0.50f, 1.00f};
		colors[ImGuiCol_CheckMark]             = {0.90f, 0.90f, 0.90f, 1.00f};
		colors[ImGuiCol_MenuBarBg]             = {0.20f, 0.20f, 0.20f, 1.00f};
		colors[ImGuiCol_SliderGrab]            = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_SliderGrabActive]      = {0.89f, 0.57f, 0.19f, 1.00f};
		colors[ImGuiCol_SeparatorHovered]      = {0.89f, 0.49f, 0.02f, 0.78f};
		colors[ImGuiCol_SeparatorActive]       = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_TabHovered]            = {0.20f, 0.20f, 0.20f, 0.81f};
		colors[ImGuiCol_Tab]                   = {0.25f, 0.25f, 0.25f, 0.86f};
		colors[ImGuiCol_TabSelected]           = {0.31f, 0.31f, 0.31f, 1.00f};
		colors[ImGuiCol_TabSelectedOverline]   = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_TabDimmed]             = {0.18f, 0.18f, 0.18f, 0.97f};
		colors[ImGuiCol_TabDimmedSelected]     = {0.25f, 0.25f, 0.25f, 1.00f};
		colors[ImGuiCol_DockingPreview]        = {0.89f, 0.49f, 0.02f, 0.70f};
		colors[ImGuiCol_TextLink]              = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_TextSelectedBg]        = {0.89f, 0.49f, 0.02f, 0.70f};
		colors[ImGuiCol_NavCursor]             = {0.89f, 0.49f, 0.02f, 1.00f};
		colors[ImGuiCol_NavWindowingHighlight] = {0.89f, 0.49f, 0.02f, 0.70f};
		colors[ImGuiCol_NavWindowingDimBg]     = {0.17f, 0.17f, 0.17f, 0.86f};
		colors[ImGuiCol_ModalWindowDimBg]      = {0.17f, 0.17f, 0.17f, 0.86f};
		colors[ImGuiCol_DragDropTarget]        = {0.89f, 0.49f, 0.02f, 0.90f};
		colors[ImGuiCol_PlotHistogram]         = {0.89f, 0.49f, 0.02f, 1.00f};

		// Main
		style.WindowPadding    = {4, 4};
		style.FramePadding     = {4, 4};
		style.ItemSpacing      = {6, 6};
		style.ItemInnerSpacing = {2, 2};
		style.IndentSpacing    = 20.0f;
		style.GrabMinSize      = 4.0f;

		// Borders
		style.WindowBorderSize = 1.0f;
		style.ChildBorderSize  = 1.0f;
		style.PopupBorderSize  = 1.0f;
		style.FrameBorderSize  = 0.0f;

		// Rounding
		style.WindowRounding = 4.0f;
		style.ChildRounding  = 4.0f;
		style.FrameRounding  = 4.0f;
		style.PopupRounding  = 8.0f;
		style.GrabRounding   = 12.0f;

		// Scrollbar
		style.ScrollbarSize     = 14.0f;
		style.ScrollbarRounding = 8.0f;
		style.ScrollbarPadding  = 0.0f;

		// Tabs
		style.TabBorderSize      = 1.0f;
		style.TabBarBorderSize   = 1.0f;
		style.TabBarOverlineSize = 2.0f;
		style.TabMinWidthBase    = 1.0f;
		style.TabMinWidthShrink  = 80.0f;
		style.TabRounding        = 4.0f;

		// Tables
		style.CellPadding             = ImVec2(2, 2);
		style.TableAngledHeadersAngle = 35.0f;

		// Trees
		style.TreeLinesFlags    = ImGuiTreeNodeFlags_DrawLinesToNodes;
		style.TreeLinesSize     = 1.0f;
		style.TreeLinesRounding = 0.0f;

		// Windows
		style.WindowTitleAlign         = {0.0f, 0.5f};
		style.WindowBorderHoverPadding = 4.0f;
		style.WindowMenuButtonPosition = ImGuiDir_Left;

		// Widgets
		style.ColorButtonPosition     = ImGuiDir_Right;
		style.ButtonTextAlign         = {0.5f, 0.5f};
		style.SelectableTextAlign     = {0.0f, 0.0f};
		style.SeparatorTextBorderSize = 2.0f;
		style.ImageBorderSize         = 0.0f;

		// Docking
		style.DockingSeparatorSize = 3.0f;

		// Tooltips
		style.HoverFlagsForTooltipMouse = ImGuiHoveredFlags_DelayNone; // 絶対いらん
	}

	/// @brief ImGuiのライトテーマスタイルを設定します。
	void StyleColorsLight() {
		ImGui::StyleColorsLight();
	}

	///	@brief Drag用のスタイルカラーをプッシュします。
	/// @param bg 通常時の背景色
	/// @param bgHovered ホバー時の背景色
	/// @param bgActive アクティブ時の背景色
	void PushStyleColorForDrag(
		const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive
	) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
	}

	/// @brief アウトライン付きのテキストを描画します。
	/// @param drawList 描画リストへのポインタ
	/// @param pos テキストの位置
	/// @param text 描画するテキスト
	/// @param textColor テキストの色
	/// @param outlineColor アウトラインの
	/// @param outlineSize アウトラインのサイズ
	void TextOutlined(
		ImDrawList*   drawList,
		const ImVec2& pos,
		const char*   text,
		const ImVec4  textColor,
		const ImVec4  outlineColor,
		const float   outlineSize
	) {
		// クライアント領域の左上座標を取得
		const auto windowPos = ImGui::GetWindowPos();
		const auto clientPos = ImVec2(windowPos.x + pos.x, windowPos.y + pos.y);

		const auto outlineCol = ImGui::ColorConvertFloat4ToU32(outlineColor);
		const auto textCol    = ImGui::ColorConvertFloat4ToU32(textColor);

		drawList->AddText(
			{clientPos.x - outlineSize, clientPos.y}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x + outlineSize, clientPos.y}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x, clientPos.y - outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x, clientPos.y + outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x - outlineSize, clientPos.y - outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x + outlineSize, clientPos.y - outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x - outlineSize, clientPos.y + outlineSize}, outlineCol,
			text
		);
		drawList->AddText(
			{clientPos.x + outlineSize, clientPos.y + outlineSize}, outlineCol,
			text
		);
		drawList->AddText(clientPos, textCol, text);
	}

	bool CollapsingHeaderWithCheckbox(
		const uint32_t           icon,
		const char*              label,
		const uint64_t           id,
		bool*                    checkbox,
		HeaderMenuAction*        action,
		const bool               canMoveUp,
		const bool               canMoveDown,
		const bool               canRemove,
		const ImGuiTreeNodeFlags flags
	) {
		const std::string uniqueID   = "##" + std::to_string(id);
		auto              menuAction = HeaderMenuAction::None;

		ImGui::PushID(uniqueID.c_str());
		ImGui::BeginGroup();

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{8.0f, 8.0f});

		const bool isOpen = ImGui::CollapsingHeader(
			uniqueID.c_str(),
			flags | ImGuiTreeNodeFlags_AllowOverlap // チェックボックスと重なってもいいようにする
		);

		const float  collapsingHeaderHeight = ImGui::GetItemRectSize().y;
		const ImVec2 collapsingHeaderMin    = ImGui::GetItemRectMin();
		const ImVec2 collapsingHeaderMax    = ImGui::GetItemRectMax();

		ImGui::PopStyleVar();

		ImGui::SameLine();

		// アイコンを表示
		constexpr float iconScale = 1.5f;

		// フォントサイズを取得
		const float fontSize = ImGui::GetFontSize();
		ImVec2      iconSize = ImGui::CalcTextSize(
			Unnamed::StrUtil::ConvertToUtf8(icon).c_str()
		);
		iconSize.x *= iconScale;

		ImGui::GetWindowDrawList()->AddText(
			ImGui::GetFont(),
			fontSize * iconScale,
			ImVec2(
				ImGui::GetCursorScreenPos().x,
				ImGui::GetCursorScreenPos().y + collapsingHeaderHeight * 0.5f -
				fontSize * iconScale * 0.5f
			),
			ImGui::GetColorU32(ImGuiCol_Text),
			Unnamed::StrUtil::ConvertToUtf8(icon).c_str()
		);

		ImGui::SameLine();

		// チェックボックス用にカーソル位置を調整
		ImGui::SetCursorPos(
			ImVec2(
				ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x +
				iconSize.x,
				ImGui::GetCursorPosY() + collapsingHeaderHeight * 0.5f -
				ImGui::GetFrameHeight() * 0.5f
			)
		);

		// チェックボックスを表示
		ImGui::Checkbox("##Checkbox", checkbox);

		ImGui::SameLine();

		// ラベルを表示
		ImGui::Text(label);

		ImGui::SameLine();

		const std::string moreHoriz = Unnamed::StrUtil::ConvertToUtf8(
			kIconMoreHoriz
		);

		const float buttonWidth = ImGui::CalcTextSize(moreHoriz.c_str()).x +
		                          ImGui::GetStyle().FramePadding.x * 2.0f;
		ImGui::SetCursorPosX(
			ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x -
			ImGui::GetStyle().ItemSpacing.x * 2.0f - buttonWidth
		);
		const float headerCenterY =
			(collapsingHeaderMin.y + collapsingHeaderMax.
			 y) *
			0.5f;
		const float buttonTopY = headerCenterY - ImGui::GetFrameHeight() * 0.5f;
		ImGui::SetCursorPosY(buttonTopY - ImGui::GetWindowPos().y);

		const bool menuOpen = ImGui::Button(moreHoriz.c_str());

		if (menuOpen) {
			ImGui::OpenPopup("##HeaderMenu");
		}
		if (ImGui::BeginPopup("##HeaderMenu")) {
			if (ImGui::MenuItem("Move Up", nullptr, false, canMoveUp)) {
				menuAction = HeaderMenuAction::MoveUp;
			}
			if (ImGui::MenuItem("Move Down", nullptr, false, canMoveDown)) {
				menuAction = HeaderMenuAction::MoveDown;
			}
			if (ImGui::MenuItem("Remove", nullptr, false, canRemove)) {
				menuAction = HeaderMenuAction::Remove;
			}
			ImGui::EndPopup();
		}

		ImGui::EndGroup();
		ImGui::PopID();
		if (action != nullptr) {
			*action = menuAction;
		}
		return isOpen;
	}
}
#endif
