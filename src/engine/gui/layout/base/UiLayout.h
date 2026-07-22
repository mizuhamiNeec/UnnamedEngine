#pragma once

#include "engine/gui/UiWidget.h"
#include "engine/gui/components/UiLayoutComponents.h"

namespace Unnamed::Gui {
	class UiLinearLayoutComponent;

	class UiLayout : public UiWidget {
	public:
		UiLayout();
		~UiLayout() override;

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
