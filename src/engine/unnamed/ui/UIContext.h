#pragma once
#include <vector>

#include "UnnamedUIDrawList.h"
#include "UnnamedUIInput.h"
#include "UITheme.h"

namespace Unnamed::UI {
	class UIContext {
	public:
		void BeginFrame(const UnnamedUiInputState& inputState);
		void EndFrame();
		void SetTheme(const UITheme& theme);
		[[nodiscard]] const UITheme& GetTheme() const;

		void BeginColumn(const Vec2& position, float gap);
		void EndColumn();
		void BeginPanel(const UIRect& rect);
		void EndPanel();

		[[nodiscard]] bool Button(const std::string& label, const UIRect& rect);
		[[nodiscard]] bool Button(const std::string& label, const Vec2& size);

		[[nodiscard]] const UIDrawList& GetDrawList() const;

	private:
		struct UILayoutState {
			Vec2  cursorPosition = Vec2::zero;
			float gap            = 0.0f;
		};

		UnnamedUiInputState mInputState;
		UIDrawList          mDrawList;
		UITheme             mTheme;
		std::vector<UILayoutState> mLayoutStack;
	};
}
