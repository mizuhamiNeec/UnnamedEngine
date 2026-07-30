#pragma once

#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiLayoutComponents.h"

namespace Unnamed::Gui {
	class UiLinearLayoutComponent;

	/// @brief UiLayoutは、親領域と子widgetのsize policyから配置矩形を計算する基底契約を提供します
	class UiLayout : public UiWidget {
	public:
		UiLayout()           = default;
		~UiLayout() override = default;

		void SetPadding(const LayoutPadding& padding);
		[[nodiscard]] const LayoutPadding& GetPadding() const;

		void                SetSpacing(float spacing);
		[[nodiscard]] float GetSpacing() const;

	protected:
		void SyncLayoutComponent() const;

		LayoutPadding mPadding = {};
		float         mSpacing = 0.0f;
	};
}
