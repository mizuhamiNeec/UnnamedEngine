#include "UIContext.h"

#include <algorithm>
#include <cmath>

#include "engine/tween/TweenManager.h"
#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::UI {
	UIContext::UIContext() {
		mTweenManager = std::make_unique<TweenManager>();
	}

	void UIContext::BeginFrame(
		const UnnamedUiInputState& inputState, const float deltaTime
	) {
		// フレーム固有の描画・レイアウト状態だけを初期化し、ドラッグ中の操作対象は維持する
		mInputState = inputState;
		mDrawList.Clear();
		mLayoutStack.clear();
		mIdStack.clear();
		mDeltaTime = deltaTime;
		mTweenManager->Update(mDeltaTime);
	}

	void UIContext::EndFrame() {
		// マウス解放までアクティブ Widget を保持して、押下と解放を同じ対象へ届ける
		if (mInputState.mouseReleased) {
			ClearActiveWidget();
		}

		static bool sWarnedUnbalancedLayout = false;
		if (!mLayoutStack.empty()) {
			if (!sWarnedUnbalancedLayout) {
				Warning(
					"UI",
					"UIContext::EndFrame detected unbalanced layout stack ({}).",
					mLayoutStack.size()
				);
			}
			sWarnedUnbalancedLayout = true;
			mLayoutStack.clear();
		} else {
			sWarnedUnbalancedLayout = false;
		}

		static bool sWarnedUnbalancedIdStack = false;
		if (!mIdStack.empty()) {
			if (!sWarnedUnbalancedIdStack) {
				Warning(
					"UI",
					"UIContext::EndFrame detected unbalanced ID stack ({}).",
					mIdStack.size()
				);
			}
			sWarnedUnbalancedIdStack = true;
			mIdStack.clear();
		} else {
			sWarnedUnbalancedIdStack = false;
		}
	}

	void UIContext::SetTheme(const UITheme& theme) {
		mTheme = theme;
	}

	const UITheme& UIContext::GetTheme() const {
		return mTheme;
	}

	bool UIContext::IsActiveWidget(const std::string& id) const {
		return !id.empty() && mActiveWidgetId == id;
	}

	void UIContext::SetActiveWidget(const std::string& id) {
		mActiveWidgetId = id;
	}

	void UIContext::ClearActiveWidget() {
		mActiveWidgetId.clear();
	}

	void UIContext::PushID(const std::string& id) {
		mIdStack.emplace_back(id);
	}

	void UIContext::PushID(const uint64_t id) {
		mIdStack.emplace_back(std::to_string(id));
	}

	void UIContext::PopID() {
		static bool sWarnedEmptyIdPop = false;
		if (mIdStack.empty()) {
			if (!sWarnedEmptyIdPop) {
				Warning("UI", "UIContext::PopID called with empty stack.");
			}
			sWarnedEmptyIdPop = true;
			return;
		}

		sWarnedEmptyIdPop = false;
		mIdStack.pop_back();
	}

	std::string UIContext::MakeWidgetId(
		const std::string& type, const std::string& label
	) const {
		// 親スコープを含め、同じラベルを持つ兄弟 Widget の入力状態を分離する
		// TODO: Replace this string path with a stable hashed ID once PushID grows beyond minimal UI.
		std::string result;
		for (const std::string& scope : mIdStack) {
			if (scope.empty()) {
				continue;
			}
			if (!result.empty()) {
				result += '/';
			}
			result += scope;
		}

		if (!result.empty()) {
			result += '/';
		}
		result += type;
		result += ':';
		result += label;
		return result;
	}

	void UIContext::BeginPanel(const UIRect& rect) {
		mDrawList.AddRect(rect, mTheme.panelColor);

		if (mTheme.panelBorderWidth > 0.0f) {
			mDrawList.AddBorder(
				rect,
				mTheme.panelBorderWidth,
				mTheme.panelBorderColor
			);
		}
	}

	void UIContext::EndPanel() {
	}

	void UIContext::BeginColumn(const Vec2& position, const float gap) {
		UILayoutState layout  = {};
		layout.cursorPosition = position;
		layout.gap            = gap;
		mLayoutStack.emplace_back(layout);
	}

	void UIContext::EndColumn() {
		static bool sWarnedEmptyLayoutPop = false;
		if (mLayoutStack.empty()) {
			if (!sWarnedEmptyLayoutPop) {
				Warning("UI", "UIContext::EndColumn called with empty stack.");
			}
			sWarnedEmptyLayoutPop = true;
			return;
		}
		sWarnedEmptyLayoutPop = false;
		mLayoutStack.pop_back();
	}

	void UIContext::BeginRow(const Vec2& position, const float gap) {
		UILayoutState state;
		state.cursorPosition = position;
		state.gap            = gap;
		state.direction      = UI_LAYOUT_DIRECTION::ROW;
		mLayoutStack.emplace_back(state);
	}

	void UIContext::EndRow() {
		static bool sWarnedEmptyLayoutPop = false;
		if (mLayoutStack.empty()) {
			if (!sWarnedEmptyLayoutPop) {
				Warning("UI", "UIContext::EndColumn called with empty stack.");
			}
			sWarnedEmptyLayoutPop = true;
			return;
		}
		sWarnedEmptyLayoutPop = false;
		mLayoutStack.pop_back();
	}

	void UIContext::Label(const std::string& text) {
		if (mLayoutStack.empty()) {
			Warning(
				"UI",
				"UIContext::Label({}) called without active layout",
				text
			);
			return;
		}

		auto& [cursorPosition, gap, direction] = mLayoutStack.back();

		const Vec2 textPosition = cursorPosition;

		mDrawList.AddText(text, textPosition, mTheme.textColor,
		                  mTheme.fontSize, mTheme.fontOversampleH,
		                  mTheme.fontOversampleV);

		const float lineHeight = mFontAtlas->GetLineHeight();

		if (direction == UI_LAYOUT_DIRECTION::COLUMN) {
			cursorPosition.y += lineHeight + gap;
		} else {
			const float textWidth = mFontAtlas->MeasureTextWidth(text);
			cursorPosition.x      += textWidth + gap;
		}
	}

	void UIContext::Label(const std::string& text, const Vec2& position) {
		const Vec2 textPosition(std::round(position.x), std::round(position.y));

		mDrawList.AddText(
			text, textPosition,
			mTheme.textColor,
			mTheme.fontSize,
			mTheme.fontOversampleH,
			mTheme.fontOversampleV
		);
	}

	void UIContext::Spacer(const float size) {
		if (mLayoutStack.empty()) {
			Warning("UI", "UIContext::Spacer called without active layout");
			return;
		}

		UILayoutState& layout = mLayoutStack.back();

		const float safeSize = std::max(size, 0.0f);

		if (layout.direction == UI_LAYOUT_DIRECTION::COLUMN) {
			layout.cursorPosition.y += safeSize;
		} else {
			layout.cursorPosition.x += safeSize;
		}
	}

	void UIContext::Separator() {
		if (mLayoutStack.empty()) {
			Warning("UI", "UIContext::Separator called with empty stack.");
			return;
		}

		// 厚みなし! 閉廷!
		if (mTheme.separatorThickness <= 0.0f) {
			return;
		}

		UILayoutState& layout = mLayoutStack.back();

		if (layout.direction == UI_LAYOUT_DIRECTION::COLUMN) {
			const UIRect rect{
				.position = layout.cursorPosition,
				.size = Vec2(mTheme.separatorLength, mTheme.separatorThickness)
			};

			mDrawList.AddRect(rect, mTheme.separatorColor);
			layout.cursorPosition.y += mTheme.separatorThickness + layout.gap;
		} else {
			const UIRect rect{
				.position = layout.cursorPosition,
				.size     = Vec2(mTheme.separatorThickness, mTheme.buttonHeight)
			};

			mDrawList.AddRect(rect, mTheme.separatorColor);
			layout.cursorPosition.x += mTheme.separatorThickness + layout.gap;
		}
	}

	bool UIContext::Button(const std::string& label, const UIRect& rect) {
		const bool isHovered = rect.Contains(mInputState.mousePosition);
		const bool isPressed = isHovered && mInputState.mouseDown;
		const bool isClicked = isHovered && mInputState.mousePressed;

		const std::string widgetId = MakeWidgetId("Button", label);
		auto&             anim     = mButtonAnimationStates[widgetId];

		const float targetHover = isHovered ? 1.0f : 0.0f;
		const float targetPress = isPressed ? 1.0f : 0.0f;

		mTweenManager->CreateTo(
			anim.hoverAmount,
			targetHover,
			0.175f
		)->SetEaseCubicBezier(
			{0.2f, 0.0f},
			{0.0f, 1.0f}
		); // Material Design Standard

		mTweenManager->CreateTo(
			anim.pressAmount,
			targetPress,
			0.175f
		)->SetEaseCubicBezier(
			{0.2f, 0.0f},
			{0.0f, 1.0f}
		); // Material Design Standard

		UIColor color = Lerp(
			mTheme.buttonNormalColor,
			mTheme.buttonHoveredColor,
			anim.hoverAmount
		);

		color = Lerp(
			color,
			mTheme.buttonPressedColor,
			anim.pressAmount
		);

		const float offsetY = mTheme.buttonPressedOffsetY * anim.pressAmount;

		UIRect visualRect     = rect;
		visualRect.position.y += std::round(offsetY);

		mDrawList.AddRect(visualRect, color);

		if (mTheme.buttonBorderWidth > 0.0f) {
			mDrawList.AddBorder(
				visualRect,
				mTheme.buttonBorderWidth,
				mTheme.buttonBorderColor
			);
		}

		const float textWidth = mFontAtlas != nullptr ?
			                        mFontAtlas->MeasureTextWidth(label) :
			                        static_cast<float>(label.size()) *
			                        mTheme.fontSize * 0.5f;
		const float lineHeight = std::max(
			1.0f,
			mFontAtlas != nullptr ?
				mFontAtlas->GetLineHeight() :
				mTheme.fontSize
		);
		float textX = visualRect.position.x + mTheme.buttonTextPadding.x;
		switch (mTheme.buttonTextAlign) {
			case UI_TEXT_ALIGN::CENTER
			: textX = visualRect.position.x + (visualRect.size.x - textWidth) *
			          0.5f;
				break;
			case UI_TEXT_ALIGN::RIGHT
			: textX = visualRect.position.x + visualRect.size.x - textWidth -
			          mTheme.buttonTextPadding.x;
				break;
			case UI_TEXT_ALIGN::LEFT:
			default: break;
		}
		const Vec2 textPosition(
			textX,
			visualRect.position.y + (visualRect.size.y - lineHeight) * 0.5f
		);

		mDrawList.AddText(
			label,
			textPosition,
			mTheme.textColor,
			mTheme.fontSize,
			mTheme.fontOversampleH,
			mTheme.fontOversampleV
		);

		return isClicked;
	}

	bool UIContext::Button(const std::string& label, const Vec2& size) {
		static bool sWarnedButtonWithoutLayout = false;
		if (mLayoutStack.empty()) {
			if (!sWarnedButtonWithoutLayout) {
				Warning(
					"UI",
					"UIContext::Button(label, size) requires an active column layout."
				);
			}
			sWarnedButtonWithoutLayout = true;
			return Button(label, UIRect{.position = Vec2::zero, .size = size});
		}
		sWarnedButtonWithoutLayout = false;

		UILayoutState& layout = mLayoutStack.back();
		const UIRect   rect{
			.position = layout.cursorPosition,
			.size     = size,
		};
		const bool clicked = Button(label, rect);
		if (layout.direction == UI_LAYOUT_DIRECTION::COLUMN) {
			layout.cursorPosition.y += size.y + layout.gap;
		} else {
			layout.cursorPosition.x += size.x + layout.gap;
		}
		return clicked;
	}

	bool UIContext::Checkbox(const std::string& label, bool* value) {
		if (value == nullptr) {
			Warning(
				"UI",
				"UIContext::Checkbox({}) called with null value pointer.",
				label
			);
			return false;
		}

		if (mLayoutStack.empty()) {
			Warning(
				"UI",
				"UIContext::Checkbox({}) called without active layout.",
				label
			);
			return false;
		}

		if (mFontAtlas == nullptr) {
			Warning(
				"UI",
				"UIContext::Checkbox({}) called without font atlas.",
				label
			);
			return false;
		}

		const float textWidth = mFontAtlas->MeasureTextWidth(label);

		UILayoutState& layout = mLayoutStack.back();

		const UIRect rect{
			.position = layout.cursorPosition,
			.size     = Vec2(
				mTheme.checkboxSize + mTheme.checkboxLabelGap + textWidth,
				mTheme.checkboxHeight
			)
		};

		const bool isHovered = rect.Contains(mInputState.mousePosition);
		const bool isPressed = isHovered && mInputState.mouseDown;
		const bool isClicked = isHovered && mInputState.mousePressed;

		const std::string widgetId = MakeWidgetId("Checkbox", label);
		auto&             anim     = mCheckBoxAnimationStates[widgetId];

		const float targetHover = isHovered ? 1.0f : 0.0f;
		const float targetPress = isPressed ? 1.0f : 0.0f;

		mTweenManager->CreateTo(
			anim.hoverAmount,
			targetHover,
			0.175f
		)->SetEaseCubicBezier(
			{0.2f, 0.0f},
			{0.0f, 1.0f}
		); // Material Design Standard

		mTweenManager->CreateTo(
			anim.pressAmount,
			targetPress,
			0.175f
		)->SetEaseCubicBezier(
			{0.2f, 0.0f},
			{0.0f, 1.0f}
		); // Material Design Standard

		UIColor color = Lerp(
			mTheme.checkboxBoxColor,
			mTheme.checkboxBoxHoveredColor,
			anim.hoverAmount
		);

		color = Lerp(
			color,
			mTheme.checkboxBoxPressedColor,
			anim.pressAmount
		);

		const float offsetY = mTheme.buttonPressedOffsetY * anim.pressAmount;

		const float boxY = rect.position.y + (rect.size.y - mTheme.checkboxSize)
		                   * 0.5f;

		const UIRect boxRect{
			.position = Vec2(rect.position.x, boxY),
			.size     = Vec2(mTheme.checkboxSize, mTheme.checkboxSize)
		};

		UIRect visualRect     = boxRect;
		visualRect.position.y += std::round(offsetY);

		mDrawList.AddRect(visualRect, color);

		mDrawList.AddBorder(
			visualRect,
			mTheme.checkboxBorderWidth,
			mTheme.checkboxBorderColor
		);

		if (*value) {
			const float padding = mTheme.checkboxCheckPadding;

			const UIRect checkRect{
				.position = visualRect.position + Vec2(padding, padding),
				.size = visualRect.size - Vec2(padding * 2.0f, padding * 2.0f)
			};

			mDrawList.AddRect(checkRect, mTheme.checkboxCheckColor);
		}

		const float lineHeight = mFontAtlas->GetLineHeight();

		const Vec2 textPosition(
			visualRect.position.x + visualRect.size.x + mTheme.checkboxLabelGap,
			visualRect.position.y + (rect.size.y - lineHeight) * 0.5f
		);

		mDrawList.AddText(
			label,
			textPosition,
			mTheme.textColor,
			mTheme.fontSize,
			mTheme.fontOversampleH,
			mTheme.fontOversampleV
		);

		bool changed = false;

		if (isClicked) {
			*value  = !*value;
			changed = true;
		}

		if (layout.direction == UI_LAYOUT_DIRECTION::COLUMN) {
			layout.cursorPosition.y += rect.size.y + layout.gap;
		} else {
			layout.cursorPosition.x += rect.size.x + layout.gap;
		}

		return changed;
	}

	bool UIContext::SliderFloat(
		const std::string& label,
		float*             value,
		float              min,
		float              max
	) {
		if (value == nullptr) {
			Warning(
				"UI",
				"UIContext::SliderFloat({}) called with null value pointer.",
				label
			);
			return false;
		}

		if (mLayoutStack.empty()) {
			Warning(
				"UI",
				"UIContext::SliderFloat({}) called without active layout.",
				label
			);
			return false;
		}

		const float valueRange = max - min;
		if (std::abs(valueRange) <= FLT_EPSILON) {
			Warning(
				"UI",
				"UIContext::SliderFloat({}) called with invalid range. min={}, max={}.",
				label,
				min,
				max
			);
			return false;
		}

		UILayoutState& layout = mLayoutStack.back();

		const float totalWidth =
			mTheme.sliderLabelWidth +
			mTheme.sliderGap +
			mTheme.sliderWidth +
			mTheme.sliderGap +
			mTheme.sliderValueWidth;

		const UIRect rect{
			.position = layout.cursorPosition,
			.size     = Vec2(totalWidth, mTheme.sliderHeight)
		};

		const float trackY =
			rect.position.y + (rect.size.y - mTheme.sliderTrackHeight) * 0.5f;

		const UIRect trackRect{
			.position = Vec2(
				rect.position.x + mTheme.sliderLabelWidth + mTheme.sliderGap,
				trackY
			),
			.size = Vec2(mTheme.sliderWidth, mTheme.sliderTrackHeight)
		};

		const std::string widgetId = MakeWidgetId("SliderFloat", label);
		const bool isHovered = trackRect.Contains(mInputState.mousePosition);
		if (isHovered && mInputState.mousePressed) {
			SetActiveWidget(widgetId);
		}

		const bool isActive  = IsActiveWidget(widgetId);
		const bool isEditing = isActive && mInputState.mouseDown;

		auto& anim = mButtonAnimationStates[widgetId];

		const float targetHover = isHovered ? 1.0f : 0.0f;
		const float targetPress = isEditing ? 1.0f : 0.0f;

		mTweenManager->CreateTo(
			anim.hoverAmount,
			targetHover,
			0.175f
		)->SetEaseCubicBezier(
			{0.2f, 0.0f},
			{0.0f, 1.0f}
		); // Material Design Standard

		mTweenManager->CreateTo(
			anim.pressAmount,
			targetPress,
			0.175f
		)->SetEaseCubicBezier(
			{0.2f, 0.0f},
			{0.0f, 1.0f}
		); // Material Design Standard

		UIColor color = Lerp(
			mTheme.checkboxBoxColor,
			mTheme.checkboxBoxHoveredColor,
			anim.hoverAmount
		);

		color = Lerp(
			color,
			mTheme.checkboxBoxPressedColor,
			anim.pressAmount
		);

		bool changed = false;

		if (isEditing) {
			const float normalized = std::clamp(
				(mInputState.mousePosition.x - trackRect.position.x) / trackRect
				.size.x,
				0.0f,
				1.0f
			);

			const float newValue = std::clamp(
				min + normalized * valueRange,
				min,
				max
			);

			if (std::abs(*value - newValue) > FLT_EPSILON) {
				*value  = newValue;
				changed = true;
			}
		}

		const float valueT = std::clamp(
			(*value - min) / valueRange,
			0.0f,
			1.0f
		);

		const UIRect fillRect{
			.position = trackRect.position,
			.size     = Vec2(trackRect.size.x * valueT, trackRect.size.y)
		};

		UIColor fillColor = mTheme.sliderFillColor;

		if (isActive) {
			fillColor = mTheme.sliderPressedFillColor;
		} else if (isHovered) {
			fillColor = mTheme.sliderHoveredFillColor;
		}

		mDrawList.AddRect(trackRect, mTheme.sliderTrackColor);
		mDrawList.AddRect(fillRect, fillColor);

		if (mTheme.sliderBorderWidth > 0.0f) {
			mDrawList.AddBorder(
				trackRect,
				mTheme.sliderBorderWidth,
				mTheme.sliderBorderColor
			);
		}

		const float lineHeight = mFontAtlas->GetLineHeight();

		const Vec2 labelPosition{
			rect.position.x,
			rect.position.y + (rect.size.y - lineHeight) * 0.5f
		};

		mDrawList.AddText(
			label,
			labelPosition,
			mTheme.textColor,
			mTheme.fontSize,
			mTheme.fontOversampleH,
			mTheme.fontOversampleV
		);

		const std::string valueText = std::format("{:.2f}", *value);

		const Vec2 valuePosition{
			trackRect.position.x + trackRect.size.x + mTheme.sliderGap,
			rect.position.y + (rect.size.y - lineHeight) * 0.5f
		};

		mDrawList.AddText(
			valueText,
			valuePosition,
			mTheme.sliderTextColor,
			mTheme.fontSize,
			mTheme.fontOversampleH,
			mTheme.fontOversampleV
		);

		if (layout.direction == UI_LAYOUT_DIRECTION::COLUMN) {
			layout.cursorPosition.y += rect.size.y + layout.gap;
		} else {
			layout.cursorPosition.x += rect.size.x + layout.gap;
		}

		return changed;
	}

	const UIDrawList& UIContext::GetDrawList() const {
		return mDrawList;
	}

	void UIContext::SetFontAtlas(UIFontAtlas* atlas) {
		mFontAtlas = atlas;
	}
}
