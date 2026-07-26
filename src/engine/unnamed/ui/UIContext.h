#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "UIFontAtlas.h"
#include "UnnamedUIDrawList.h"
#include "UnnamedUIInput.h"
#include "UITheme.h"

#include "engine/tween/TweenManager.h"

namespace Unnamed::UI {
	/// @brief UIContextは、即時UIの入力、レイアウトスタック、操作状態、描画リストをフレーム単位で管理します
	class UIContext {
	public:
		UIContext();

		void BeginFrame(const UnnamedUiInputState& inputState, float deltaTime);
		void EndFrame();
		void SetTheme(const UITheme& theme);
		[[nodiscard]] const UITheme& GetTheme() const;
		[[nodiscard]] bool IsActiveWidget(const std::string& id) const;
		void SetActiveWidget(const std::string& id);
		void ClearActiveWidget();

		/// @brief 後続Widget IDのscopeを追加します。
		void PushID(const std::string& id);

		/// @brief 数値IDを文字列scopeとして後続Widget IDに追加します。
		void PushID(uint64_t id);

		/// @brief 現在のWidget ID scopeを1つ戻します。
		void PopID();

		/// @brief 現在のID stackを含めたWidget ID文字列を生成します。
		[[nodiscard]] std::string MakeWidgetId(
			const std::string& type, const std::string& label
		) const;

		void BeginPanel(const UIRect& rect);
		void EndPanel();

		void BeginColumn(const Vec2& position, float gap);
		void EndColumn();

		void BeginRow(const Vec2& position, float gap);
		void EndRow();

		void Label(const std::string& text);
		void Label(const std::string& text, const Vec2& position);

		void Spacer(float size);

		void Separator();

		[[nodiscard]] bool Button(const std::string& label, const UIRect& rect);
		[[nodiscard]] bool Button(const std::string& label, const Vec2& size);

		[[nodiscard]] bool Checkbox(const std::string& label, bool* value);

		[[nodiscard]] bool SliderFloat(
			const std::string& label, float* value, float min, float max
		);

		[[nodiscard]] const UIDrawList& GetDrawList() const;
		void                            SetFontAtlas(UIFontAtlas* atlas);

	private:
		enum class UI_LAYOUT_DIRECTION {
			COLUMN,
			ROW
		};

		/// @brief UILayoutStateは、即時UIのcursor位置、利用可能幅、行高さをlayout stack内で保持します
		struct UILayoutState {
			Vec2                cursorPosition = Vec2::zero;
			float               gap            = 0.0f;
			UI_LAYOUT_DIRECTION direction      = UI_LAYOUT_DIRECTION::COLUMN;
		};

		/// @brief UIButtonAnimationStateは、即時buttonのhover・press補間値をwidget IDごとに保持します
		struct UIButtonAnimationState {
			float hoverAmount = 0.0f;
			float pressAmount = 0.0f;
		};

		/// @brief UICheckBoxAnimationStateは、即時checkboxのhover・check補間値をwidget IDごとに保持します
		struct UICheckBoxAnimationState {
			float hoverAmount = 0.0f;
			float pressAmount = 0.0f;
		};

		/// @brief UISliderAnimationStateは、即時sliderのhover・grab補間値をwidget IDごとに保持します
		struct UISliderAnimationState {
			float hoverAmount = 0.0f;
			float pressAmount = 0.0f;
		};

		UnnamedUiInputState      mInputState;
		std::string              mActiveWidgetId;
		std::vector<std::string> mIdStack;

		UIFontAtlas* mFontAtlas = nullptr;

		UIDrawList                 mDrawList;
		UITheme                    mTheme;
		std::vector<UILayoutState> mLayoutStack;

		std::unordered_map<std::string, UIButtonAnimationState>
		mButtonAnimationStates;
		std::unordered_map<std::string, UICheckBoxAnimationState>
		mCheckBoxAnimationStates;
		std::unordered_map<std::string, UISliderAnimationState>
		mSliderAnimationStates;

		std::unique_ptr<TweenManager> mTweenManager;

		float mDeltaTime = 0.0f;
	};
}
