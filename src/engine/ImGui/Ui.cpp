#ifdef _DEBUG
#include "Ui.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cfloat>
#include <string>

#include "engine/Properties.h"

namespace Ui {
	namespace {
		constexpr uint32_t                             kMaxRowDepth     = 64;
		thread_local std::array<float, kMaxRowDepth>   sRowSpacingStack = {};
		thread_local std::array<uint8_t, kMaxRowDepth> sRowHasItemStack = {};
		thread_local uint32_t                          sRowDepth        = 0;

		UiContext& MutableContext() {
			static UiContext context;
			return context;
		}

		[[nodiscard]] uint32_t ClampRowIndex(const uint32_t depth) {
			if (depth == 0) {
				return 0;
			}
			return std::min(depth - 1, kMaxRowDepth - 1);
		}

		[[nodiscard]] int EncodeCodepointToUtf8(
			const uint32_t codepoint,
			char           out[5]
		) {
			if (codepoint <= 0x7Fu) {
				out[0] = static_cast<char>(codepoint);
				out[1] = '\0';
				return 1;
			}
			if (codepoint <= 0x7FFu) {
				out[0] = static_cast<char>(0xC0u | codepoint >> 6u);
				out[1] = static_cast<char>(0x80u | codepoint & 0x3Fu);
				out[2] = '\0';
				return 2;
			}
			if (codepoint <= 0xFFFFu) {
				out[0] = static_cast<char>(0xE0u | codepoint >> 12u);
				out[1] = static_cast<char>(
					0x80u | codepoint >> 6u & 0x3Fu
				);
				out[2] = static_cast<char>(0x80u | codepoint & 0x3Fu);
				out[3] = '\0';
				return 3;
			}
			if (codepoint <= 0x10FFFFu) {
				out[0] = static_cast<char>(0xF0u | codepoint >> 18u);
				out[1] = static_cast<char>(
					0x80u | codepoint >> 12u & 0x3Fu
				);
				out[2] = static_cast<char>(
					0x80u | codepoint >> 6u & 0x3Fu
				);
				out[3] = static_cast<char>(0x80u | codepoint & 0x3Fu);
				out[4] = '\0';
				return 4;
			}

			// 無効なコードポイントは '?' にフォールバックします。
			out[0] = '?';
			out[1] = '\0';
			return 1;
		}

		class ScopedRowContextIsolation {
		public:
			ScopedRowContextIsolation() {
				mSavedDepth = sRowDepth;
				sRowDepth   = 0;
			}

			~ScopedRowContextIsolation() {
				sRowDepth = mSavedDepth;
			}

			ScopedRowContextIsolation(
				const ScopedRowContextIsolation&
			) = delete;
			ScopedRowContextIsolation& operator=(
				const ScopedRowContextIsolation&
			
			) = delete;

		private:
			uint32_t mSavedDepth = 0;
		};

		[[nodiscard]] ImDrawFlags ToDrawFlags(const UiCornerMode cornerMode) {
			switch (cornerMode) {
				case UiCornerMode::TopOnly: return ImDrawFlags_RoundCornersTop;
				case UiCornerMode::BottomOnly: return
						ImDrawFlags_RoundCornersBottom;
				case UiCornerMode::All: return ImDrawFlags_RoundCornersAll;
				case UiCornerMode::None:
				default: return ImDrawFlags_RoundCornersNone;
			}
		}

		void BeginRow(const float spacing) {
			if (sRowDepth < kMaxRowDepth) {
				sRowSpacingStack[sRowDepth] = spacing;
				sRowHasItemStack[sRowDepth] = 0;
			}
			++sRowDepth;
		}

		void EndRow() {
			if (sRowDepth > 0) {
				--sRowDepth;
			}
		}

		void BeforeLayoutItem() {
			if (sRowDepth == 0) {
				return;
			}

			const uint32_t rowIndex   = ClampRowIndex(sRowDepth);
			uint8_t&       rowHasItem = sRowHasItemStack[rowIndex];
			if (rowHasItem) {
				const float spacing = sRowSpacingStack[rowIndex];
				if (spacing >= 0.0f) {
					ImGui::SameLine(0.0f, spacing);
				} else {
					ImGui::SameLine();
				}
			}
			rowHasItem = 1;
		}

		[[nodiscard]] ImU32 ResolveColorFromStyleSet(
			const UiStyleSet& styleSet,
			const UiColorRole colorRole
		) {
			switch (colorRole) {
				case UiColorRole::TextMuted: return
						ImGui::ColorConvertFloat4ToU32(styleSet.textMuted);
				case UiColorRole::Accent: return ImGui::ColorConvertFloat4ToU32(
						styleSet.accent);
				case UiColorRole::SectionHeaderBackground: return
						ImGui::ColorConvertFloat4ToU32(
							styleSet.sectionHeaderBackground
						);
				case UiColorRole::SectionHeaderText: return
						ImGui::ColorConvertFloat4ToU32(
							styleSet.sectionHeaderText
						);
				case UiColorRole::Text:
				default: return ImGui::ColorConvertFloat4ToU32(styleSet.text);
			}
		}
	}

	ScopedId::ScopedId(const char* id) {
		ImGui::PushID(id);
		mActive = true;
	}

	ScopedId::ScopedId(const int32_t id) {
		ImGui::PushID(id);
		mActive = true;
	}

	ScopedId::ScopedId(const uint64_t id) {
		ImGui::PushID(
			reinterpret_cast<const void*>(static_cast<uintptr_t>(id))
		);
		mActive = true;
	}

	ScopedId::~ScopedId() {
		if (mActive) {
			ImGui::PopID();
		}
	}

	ScopedId::ScopedId(ScopedId&& other) noexcept {
		mActive       = other.mActive;
		other.mActive = false;
	}

	ScopedId& ScopedId::operator=(ScopedId&& other) noexcept {
		if (this != &other) {
			if (mActive) {
				ImGui::PopID();
			}
			mActive       = other.mActive;
			other.mActive = false;
		}
		return *this;
	}

	ScopedStyleVar::ScopedStyleVar(
		const ImGuiStyleVar styleVar, const float value
	) {
		ImGui::PushStyleVar(styleVar, value);
		mActive = true;
	}

	ScopedStyleVar::ScopedStyleVar(
		const ImGuiStyleVar styleVar, const ImVec2& value
	) {
		ImGui::PushStyleVar(styleVar, value);
		mActive = true;
	}

	ScopedStyleVar::~ScopedStyleVar() {
		if (mActive) {
			ImGui::PopStyleVar();
		}
	}

	ScopedStyleVar::ScopedStyleVar(ScopedStyleVar&& other) noexcept {
		mActive       = other.mActive;
		other.mActive = false;
	}

	ScopedStyleVar& ScopedStyleVar::operator=(
		ScopedStyleVar&& other
	) noexcept {
		if (this != &other) {
			if (mActive) {
				ImGui::PopStyleVar();
			}
			mActive       = other.mActive;
			other.mActive = false;
		}
		return *this;
	}

	ScopedStyleColor::ScopedStyleColor(
		const ImGuiCol colorIndex, const ImVec4& color
	) {
		ImGui::PushStyleColor(colorIndex, color);
		mActive = true;
	}

	ScopedStyleColor::ScopedStyleColor(
		const ImGuiCol colorIndex, const ImU32 color
	) {
		ImGui::PushStyleColor(colorIndex, color);
		mActive = true;
	}

	ScopedStyleColor::~ScopedStyleColor() {
		if (mActive) {
			ImGui::PopStyleColor();
		}
	}

	ScopedStyleColor::ScopedStyleColor(ScopedStyleColor&& other) noexcept {
		mActive       = other.mActive;
		other.mActive = false;
	}

	ScopedStyleColor& ScopedStyleColor::operator=(
		ScopedStyleColor&& other
	) noexcept {
		if (this != &other) {
			if (mActive) {
				ImGui::PopStyleColor();
			}
			mActive       = other.mActive;
			other.mActive = false;
		}
		return *this;
	}

	ScopedFont::ScopedFont(ImFont* font) {
		if (font != nullptr) {
			ImGui::PushFont(font);
			mActive = true;
		}
	}

	ScopedFont::~ScopedFont() {
		if (mActive) {
			ImGui::PopFont();
		}
	}

	ScopedFont::ScopedFont(ScopedFont&& other) noexcept {
		mActive       = other.mActive;
		other.mActive = false;
	}

	ScopedFont& ScopedFont::operator=(ScopedFont&& other) noexcept {
		if (this != &other) {
			if (mActive) {
				ImGui::PopFont();
			}
			mActive       = other.mActive;
			other.mActive = false;
		}
		return *this;
	}

	ScopedDisabled::ScopedDisabled(const bool disabled) {
		if (disabled) {
			ImGui::BeginDisabled();
			mActive = true;
		}
	}

	ScopedDisabled::~ScopedDisabled() {
		if (mActive) {
			ImGui::EndDisabled();
		}
	}

	ScopedDisabled::ScopedDisabled(ScopedDisabled&& other) noexcept {
		mActive       = other.mActive;
		other.mActive = false;
	}

	ScopedDisabled& ScopedDisabled::operator=(
		ScopedDisabled&& other
	) noexcept {
		if (this != &other) {
			if (mActive) {
				ImGui::EndDisabled();
			}
			mActive       = other.mActive;
			other.mActive = false;
		}
		return *this;
	}

	void SetContext(const UiContext& context) {
		MutableContext() = context;
	}

	const UiContext& GetContext() {
		return MutableContext();
	}

	void ResetContext() {
		MutableContext() = UiContext{};
	}

	ImU32 ResolveColor(const UiColorRole colorRole) {
		const UiContext& context = GetContext();
		if (context.colorResolver) {
			return context.colorResolver(colorRole);
		}
		return ResolveColorFromStyleSet(context.styleSet, colorRole);
	}

	ImFont* ResolveFont(const UiFontStyle fontStyle) {
		const UiContext& context = GetContext();
		if (context.fontResolver) {
			if (ImFont* font = context.fontResolver(fontStyle)) {
				return font;
			}
		}
		return ImGui::GetFont();
	}

	float ResolveFontSize(
		const UiFontStyle fontStyle, const float requestedSize
	) {
		if (requestedSize > 0.0f) {
			return requestedSize;
		}

		const UiStyleSet& styleSet = GetContext().styleSet;
		const float       baseSize = ImGui::GetFontSize();
		switch (fontStyle) {
			case UiFontStyle::Heading: return
					baseSize * styleSet.headingFontScale;
			case UiFontStyle::Caption: return
					baseSize * styleSet.captionFontScale;
			case UiFontStyle::Body:
			default: return baseSize;
		}
	}

	void Row(const std::function<void()>& drawContent, const float spacing) {
		// Row自体をレイアウト要素として扱い、親Row内でも横並びを維持します。
		BeforeLayoutItem();
		ImGui::BeginGroup();
		BeginRow(spacing);
		drawContent();
		EndRow();
		ImGui::EndGroup();
	}

	void Centered(
		const std::function<void()>& drawContent, const float contentWidth
	) {
		BeforeLayoutItem();

		if (contentWidth > 0.0f) {
			const float availableWidth = ImGui::GetContentRegionAvail().x;
			const float offsetX        = std::max(
				0.0f,
				(availableWidth - contentWidth) * 0.5f
			);
			if (offsetX > 0.0f) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
			}
		}

		drawContent();
	}

	void AlignCenterY(
		const float containerHeight, const float contentHeight
	) {
		const float targetContainerHeight = containerHeight > 0.0f ?
			                                    containerHeight :
			                                    ImGui::GetContentRegionAvail().
			                                    y;
		const float targetContentHeight = contentHeight > 0.0f ?
			                                  contentHeight :
			                                  ImGui::GetFrameHeight();
		const float offsetY = std::max(
			0.0f,
			(targetContainerHeight - targetContentHeight) * 0.5f
		);

		if (offsetY > 0.0f) {
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + offsetY);
		}
	}

	void CenteredY(
		const std::function<void()>& drawContent,
		const float                  containerHeight,
		const float                  contentHeight
	) {
		BeforeLayoutItem();
		ImGui::BeginGroup();
		const ScopedRowContextIsolation isolateRowContext;

		if (containerHeight <= 0.0f) {
			AlignCenterY(0.0f, contentHeight);
			drawContent();
			ImGui::EndGroup();
			return;
		}

		// 親Row内でも各要素の高さをそろえるため、固定高さのグループとして扱います。
		const float slotStartY = ImGui::GetCursorPosY();
		AlignCenterY(containerHeight, contentHeight);
		drawContent();
		const float slotEndY        = slotStartY + containerHeight;
		const float remainingHeight = slotEndY - ImGui::GetCursorPosY();
		if (remainingHeight > 0.0f) {
			// SetCursorPosだけで境界を拡張せず、Dummyで安全にスロット終端まで進めます。
			ImGui::Dummy(ImVec2(0.0f, remainingHeight));
		}
		ImGui::EndGroup();
	}

	void Section(
		const char*                  sectionLabel,
		const std::function<void()>& drawContent,
		const bool                   drawTrailingSeparator
	) {
		ImGui::BeginGroup();
		if (sectionLabel != nullptr && sectionLabel[0] != '\0') {
			SectionHeader(sectionLabel);
			Spacing(GetContext().styleSet.sectionContentSpacingY);
		}
		drawContent();
		if (drawTrailingSeparator) {
			Spacing(GetContext().styleSet.sectionContentSpacingY);
			Separator();
		}
		ImGui::EndGroup();
	}

	void Spacing(const float height) {
		if (height > 0.0f) {
			ImGui::Dummy(ImVec2(0.0f, height));
			return;
		}
		ImGui::Spacing();
	}

	void Separator() {
		ImGui::Separator();
	}

	void Text(const std::string_view text) {
		BeforeLayoutItem();
		ImGui::TextUnformatted(text.data(), text.data() + text.size());
	}

	void TextWrapped(const std::string_view text) {
		BeforeLayoutItem();
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextUnformatted(text.data(), text.data() + text.size());
		ImGui::PopTextWrapPos();
	}

	void DrawTextW(const UiTextDrawParams& params) {
		if (params.text.empty()) {
			return;
		}

		ImDrawList* drawList = params.drawList ?
			                       params.drawList :
			                       ImGui::GetWindowDrawList();
		if (drawList == nullptr) {
			return;
		}

		ImFont* font = params.font ?
			               params.font :
			               ResolveFont(params.fontStyle);
		const float fontSize = ResolveFontSize(params.fontStyle,
		                                       params.fontSize);
		const ImU32 textColor = params.useExplicitColor ?
			                        params.color :
			                        ResolveColor(params.colorRole);

		ImVec2 screenPos = params.position;
		if (!params.positionIsScreenSpace) {
			const ImVec2 windowPos = ImGui::GetWindowPos();
			screenPos              = ImVec2(
				windowPos.x + params.position.x,
				windowPos.y + params.position.y
			);
		}

		// Fontを明示した場合だけfont+fontSize版を使い、通常は現行フォントで軽量に描画します。
		if (font != nullptr) {
			drawList->AddText(
				font,
				fontSize,
				screenPos,
				textColor,
				params.text.data(),
				params.text.data() + params.text.size()
			);
		} else {
			drawList->AddText(
				screenPos,
				textColor,
				params.text.data(),
				params.text.data() + params.text.size()
			);
		}
	}

	bool Button(const char* label, const ImVec2& size) {
		BeforeLayoutItem();
		return ImGui::Button(label, size);
	}

	void PushStyleColorForDrag(
		const ImVec4& bg, const ImVec4& bgHovered, const ImVec4& bgActive
	) {
		ImGui::PushStyleColor(ImGuiCol_FrameBg, bg);
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, bgHovered);
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, bgActive);
	}

	bool IconButton(
		const uint32_t icon,
		const char*    label,
		ImVec2         size,
		const float    iconScale,
		const ImGuiDir labelDir
	) {
		BeforeLayoutItem();

		const ImGuiStyle& style       = ImGui::GetStyle();
		ImDrawList*       drawList    = ImGui::GetWindowDrawList();
		ImFont*           font        = ImGui::GetFont();
		char              iconUtf8[5] = {};
		(void)EncodeCodepointToUtf8(icon, iconUtf8);
		const bool hasLabel         = label && label[0] != '\0';
		const bool horizontalLayout =
			labelDir == ImGuiDir_Left || labelDir == ImGuiDir_Right;
		const bool  iconOnly   = !hasLabel || labelDir == ImGuiDir_None;
		const bool  autoWidth  = size.x <= 0.0f;
		const bool  autoHeight = size.y <= 0.0f;
		const float gap        = horizontalLayout ?
			                         style.ItemSpacing.x :
			                         style.ItemSpacing.y;
		const ImVec2 pad              = style.FramePadding;
		const float  baseFontSize     = ImGui::GetFontSize();
		const float  baseFontSizeSafe = std::max(1.0f, baseFontSize);
		const ImVec2 iconBaseSize     = ImGui::CalcTextSize(iconUtf8);
		const ImVec2 labelSize        = hasLabel ?
			                                ImGui::CalcTextSize(label) :
			                                ImVec2(0.0f, 0.0f);
		const float autoIconFontSize = std::max(1.0f, baseFontSize * iconScale);
		const float autoFontScale    = autoIconFontSize / baseFontSizeSafe;
		ImVec2      layoutIconSize   = ImVec2(
			iconBaseSize.x * autoFontScale,
			iconBaseSize.y * autoFontScale
		);

		// 明示サイズがある場合はそれに合わせてアイコンの実サイズを再計算します。
		if (!autoHeight) {
			const float explicitInnerHeight = std::max(
				1.0f,
				size.y - pad.y * 2.0f
			);
			const float explicitIconFontSize = std::max(
				1.0f,
				explicitInnerHeight * iconScale
			);
			const float explicitFontScale = explicitIconFontSize /
			                                baseFontSizeSafe;
			layoutIconSize = ImVec2(
				iconBaseSize.x * explicitFontScale,
				iconBaseSize.y * explicitFontScale
			);
		}

		// ラベル方向と自動サイズ有無に応じて最終ボタンサイズを確定します。
		if (autoWidth) {
			if (iconOnly) {
				size.x = layoutIconSize.x + pad.x * 2.0f;
			} else if (horizontalLayout) {
				size.x =
					layoutIconSize.x + labelSize.x + gap + pad.x * 2.0f;
			} else {
				size.x =
					std::max(layoutIconSize.x, labelSize.x) + pad.x * 2.0f;
			}
		}

		if (autoHeight) {
			if (iconOnly) {
				size.y = layoutIconSize.y + pad.y * 2.0f;
			} else if (horizontalLayout) {
				size.y =
					std::max(layoutIconSize.y, labelSize.y) + pad.y * 2.0f;
			} else {
				size.y =
					layoutIconSize.y + labelSize.y + gap + pad.y * 2.0f;
			}
		}

		// InvisibleButtonで入力を処理し、描画はDrawListで制御します。
		ImGui::PushID(static_cast<int>(icon));
		if (hasLabel) {
			ImGui::PushID(label);
		}
		const bool pressed = ImGui::InvisibleButton("##IconButton", size);
		const bool hovered = ImGui::IsItemHovered();
		const bool active  = ImGui::IsItemActive();
		if (hasLabel) {
			ImGui::PopID();
		}
		ImGui::PopID();

		const ImVec2 itemMin  = ImGui::GetItemRectMin();
		const ImVec2 itemMax  = ImGui::GetItemRectMax();
		const ImVec2 itemSize = ImVec2(
			itemMax.x - itemMin.x,
			itemMax.y - itemMin.y
		);
		const ImVec2 innerMin  = ImVec2(itemMin.x + pad.x, itemMin.y + pad.y);
		const ImVec2 innerSize = ImVec2(
			std::max(1.0f, itemSize.x - pad.x * 2.0f),
			std::max(1.0f, itemSize.y - pad.y * 2.0f)
		);

		const ImU32 bgColor = ImGui::GetColorU32(
			active ?
				ImGuiCol_ButtonActive :
				hovered ?
				ImGuiCol_ButtonHovered :
				ImGuiCol_Button
		);
		drawList->AddRectFilled(
			itemMin,
			itemMax,
			bgColor,
			style.FrameRounding,
			ImDrawFlags_RoundCornersAll
		);
		if (style.FrameBorderSize > 0.0f) {
			drawList->AddRect(
				itemMin,
				itemMax,
				ImGui::GetColorU32(ImGuiCol_Border),
				style.FrameRounding,
				ImDrawFlags_RoundCornersAll,
				style.FrameBorderSize
			);
		}

		const float iconFontSize = autoHeight ?
			                           autoIconFontSize :
			                           std::max(1.0f, innerSize.y * iconScale);
		const float  fontScale = iconFontSize / baseFontSizeSafe;
		const ImVec2 iconSize  = ImVec2(
			iconBaseSize.x * fontScale,
			iconBaseSize.y * fontScale
		);

		// アイコンとラベルの配置位置をレイアウト方向別に決定します。
		ImVec2 iconPos  = innerMin;
		ImVec2 labelPos = innerMin;

		if (iconOnly) {
			iconPos = ImVec2(
				innerMin.x + (innerSize.x - iconSize.x) * 0.5f,
				innerMin.y + (innerSize.y - iconSize.y) * 0.5f
			);
		} else if (horizontalLayout) {
			const float blockWidth = iconSize.x + labelSize.x + gap;
			const float blockHeight = std::max(iconSize.y, labelSize.y);
			const float startX = innerMin.x + (innerSize.x - blockWidth) * 0.5f;
			const float startY = innerMin.y + (innerSize.y - blockHeight) *
			                     0.5f;

			if (labelDir == ImGuiDir_Left) {
				labelPos = ImVec2(
					startX,
					startY + (blockHeight - labelSize.y) * 0.5f
				);
				iconPos = ImVec2(
					startX + labelSize.x + gap,
					startY + (blockHeight - iconSize.y) * 0.5f
				);
			} else {
				iconPos = ImVec2(
					startX,
					startY + (blockHeight - iconSize.y) * 0.5f
				);
				labelPos = ImVec2(
					startX + iconSize.x + gap,
					startY + (blockHeight - labelSize.y) * 0.5f
				);
			}
		} else {
			const float blockHeight = iconSize.y + labelSize.y + gap;
			const float startY      = innerMin.y + (innerSize.y - blockHeight) *
			                          0.5f;
			if (labelDir == ImGuiDir_Up) {
				labelPos = ImVec2(
					innerMin.x + (innerSize.x - labelSize.x) * 0.5f,
					startY
				);
				iconPos = ImVec2(
					innerMin.x + (innerSize.x - iconSize.x) * 0.5f,
					startY + labelSize.y + gap
				);
			} else {
				iconPos = ImVec2(
					innerMin.x + (innerSize.x - iconSize.x) * 0.5f,
					startY
				);
				labelPos = ImVec2(
					innerMin.x + (innerSize.x - labelSize.x) * 0.5f,
					startY + iconSize.y + gap
				);
			}
		}

		const ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
		drawList->PushClipRect(itemMin, itemMax, true);
		drawList->AddText(
			font,
			iconFontSize,
			iconPos,
			textColor,
			iconUtf8
		);
		if (!iconOnly) {
			drawList->AddText(labelPos, textColor, label);
		}
		drawList->PopClipRect();

		return pressed;
	}

	bool BeginMainMenu(const char* label, const bool enabled) {
		ImGui::PushStyleVar(
			ImGuiStyleVar_FramePadding,
			ImVec2(
				ImGui::GetStyle().FramePadding.x,
				kTitleBarH * 0.5f - ImGui::GetFontSize() * 0.5f
			)
		);
		ImGui::PushStyleVar(
			ImGuiStyleVar_ItemSpacing,
			ImVec2(ImGui::GetStyle().ItemSpacing.x, kTitleBarH)
		);
		const bool ret = ImGui::BeginMenu(label, enabled);
		ImGui::PopStyleVar(2);
		return ret;
	}

	void ImageWithRounding(
		const ImTextureID textureId,
		const ImVec2      imageSize,
		const float       rounding,
		const ImDrawFlags flags,
		const ImVec2      uv0,
		const ImVec2      uv1,
		const ImVec4      tintColor
	) {
		BeforeLayoutItem();

		const ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::GetWindowDrawList()->AddImageRounded(
			textureId,
			pos,
			ImVec2(pos.x + imageSize.x, pos.y + imageSize.y),
			uv0,
			uv1,
			ImGui::GetColorU32(tintColor),
			rounding,
			flags
		);
		ImGui::Dummy(imageSize);
	}

	void SectionHeader(const char* text) {
		UiSectionHeaderParams params = {};
		params.text                  = text ? std::string_view(text) : "";
		SectionHeader(params);
	}

	void SectionHeader(const UiSectionHeaderParams& params) {
		BeforeLayoutItem();

		const UiContext&  context  = GetContext();
		const UiStyleSet& styleSet = context.styleSet;
		ImDrawList*       drawList = ImGui::GetWindowDrawList();

		const ImVec2 cursorPos = ImGui::GetCursorScreenPos();
		const float  width     = ImGui::GetContentRegionAvail().x;
		const ImVec2 padding   =
			params.padding.x > 0.0f || params.padding.y > 0.0f ?
				params.padding :
				styleSet.sectionHeaderPadding;

		ImFont* headerFont = params.font ?
			                     params.font :
			                     ResolveFont(params.fontStyle);
		const float headerFontSize = ResolveFontSize(
			params.fontStyle,
			params.fontSize
		);
		const float fontSizeForCalc = headerFont ?
			                              headerFontSize :
			                              ImGui::GetFontSize();
		const ImVec2 textSize = headerFont ?
			                        headerFont->CalcTextSizeA(
				                        fontSizeForCalc,
				                        FLT_MAX,
				                        0.0f,
				                        params.text.data(),
				                        params.text.data() + params.text.size()
			                        ) :
			                        ImGui::CalcTextSize(
				                        params.text.data(),
				                        params.text.data() + params.text.size()
			                        );

		const float minHeight = params.minHeight > 0.0f ?
			                        params.minHeight :
			                        styleSet.sectionHeaderMinHeight;
		const float headerHeight = std::max(
			minHeight,
			textSize.y + padding.y * 2.0f
		);
		const float rounding = params.rounding > 0.0f ?
			                       params.rounding :
			                       styleSet.sectionHeaderRounding;
		const ImVec2 rectMin = cursorPos;
		const ImVec2 rectMax = ImVec2(
			cursorPos.x + width,
			cursorPos.y + headerHeight
		);

		DrawFilledRect(
			drawList,
			rectMin,
			rectMax,
			ResolveColor(params.backgroundColorRole),
			rounding,
			params.cornerMode
		);

		UiTextDrawParams textParams = {};
		textParams.drawList         = drawList;
		textParams.position         = ImVec2(
			rectMin.x + padding.x,
			rectMin.y + (headerHeight - textSize.y) * 0.5f
		);
		textParams.text      = params.text;
		textParams.font      = headerFont;
		textParams.fontStyle = params.fontStyle;
		textParams.fontSize  = headerFontSize;
		textParams.colorRole = params.textColorRole;
		DrawTextW(textParams);

		ImGui::Dummy(ImVec2(width, headerHeight));
	}

	void DrawFilledRect(
		ImDrawList*        drawList,
		const ImVec2&      min,
		const ImVec2&      max,
		const ImU32        color,
		const float        rounding,
		const UiCornerMode cornerMode
	) {
		if (drawList == nullptr) {
			return;
		}
		drawList->AddRectFilled(
			min,
			max,
			color,
			rounding,
			ToDrawFlags(cornerMode)
		);
	}

	void DrawFilledRectTopRounded(
		ImDrawList*   drawList,
		const ImVec2& min,
		const ImVec2& max,
		const ImU32   color,
		const float   rounding
	) {
		DrawFilledRect(
			drawList,
			min,
			max,
			color,
			rounding,
			UiCornerMode::TopOnly
		);
	}
}

#endif
