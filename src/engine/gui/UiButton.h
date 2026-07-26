#pragma once

#include <functional>

#include "UiWidget.h"

namespace Unnamed::Gui {
	class UiButtonBehaviorComponent;

	/// @brief UiButtonは、押下状態とクリック通知をUiWidgetの入力処理へ追加します
	class UiButton : public UiWidget {
	public:
		UiButton();
		~UiButton() override;

		void                           SetText(const std::string_view& text);
		[[nodiscard]] std::string_view GetText() const;

		void SetOnClick(const std::function<void()>& callback);
		void SetColors(
			const Color& normal, const Color& hovered, const Color& pressed
		);

		[[nodiscard]] const char* GetTypeName() const override {
			return "ButtonPreset";
		}

	private:
		[[nodiscard]] UiButtonBehaviorComponent* GetBehavior() const;
	};
}
