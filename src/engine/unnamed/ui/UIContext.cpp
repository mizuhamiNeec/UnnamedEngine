#include "UIContext.h"

#include "engine/unnamed/subsystem/console/Log.h"

namespace Unnamed::UI {
	void UIContext::BeginFrame(const UnnamedUiInputState& inputState) {
		mInputState = inputState;
		mDrawList.Clear();
		mLayoutStack.clear();
	}

	void UIContext::SetTheme(const UITheme& theme) {
		mTheme = theme;
	}

	const UITheme& UIContext::GetTheme() const {
		return mTheme;
	}

	void UIContext::EndFrame() {
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
			return;
		}
		sWarnedUnbalancedLayout = false;
	}

	void UIContext::BeginColumn(const Vec2& position, const float gap) {
		UILayoutState layout = {};
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

	void UIContext::BeginPanel(const UIRect& rect) {
		mDrawList.AddRect(rect, mTheme.panelColor);
	}

	void UIContext::EndPanel() {}

	bool UIContext::Button(const std::string& label, const UIRect& rect) {
		const bool isHovered = rect.Contains(mInputState.mousePosition);
		const bool isPressed = isHovered && mInputState.mouseDown;

		const bool isClicked = isHovered && mInputState.mousePressed;

		const UIColor color = isPressed ? mTheme.buttonPressedColor :
		                      (isHovered ? mTheme.buttonHoveredColor :
		                                   mTheme.buttonNormalColor);

		mDrawList.AddRect(rect, color);
		mDrawList.AddText(
			label,
			rect.position + mTheme.buttonTextPadding,
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
		layout.cursorPosition.y += size.y + layout.gap;
		return clicked;
	}

	const UIDrawList& UIContext::GetDrawList() const {
		return mDrawList;
	}
}
