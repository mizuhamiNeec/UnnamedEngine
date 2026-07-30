#pragma once

#include "UiWidget.h"

namespace Unnamed::Gui {
	class UiPanelStyleComponent;

	/// @brief UiPanelは、子ウィジェットの配置領域と背景描画を提供します
	class UiPanel : public UiWidget {
	public:
		UiPanel();
		~UiPanel() override = default;

		void                       SetBackgroundColor(const Color& color) const;
		[[nodiscard]] const Color& GetBackgroundColor() const;

		void                SetCornerRadius(float radius) const;
		[[nodiscard]] float GetCornerRadius() const;

		[[nodiscard]] const char* GetTypeName() const override;

	private:
		[[nodiscard]] UiPanelStyleComponent* GetStyleComponent() const;
	};
}
