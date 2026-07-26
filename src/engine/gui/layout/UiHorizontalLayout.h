#pragma once
#include "base/UiLayout.h"

namespace Unnamed::Gui {
	/// @brief UiHorizontalLayoutは、子widgetを左から右へspacingとalignmentに従って配置します
	class UiHorizontalLayout : public UiLayout {
	public:
		UiHorizontalLayout();
		~UiHorizontalLayout() override;

		const char* GetTypeName() const override {
			return "HorizontalLayoutPreset";
		}
	};
}
