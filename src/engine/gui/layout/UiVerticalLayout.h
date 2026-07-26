#pragma once
#include "base/UiLayout.h"

namespace Unnamed::Gui {
	/// @brief UiVerticalLayoutは、子widgetを上から下へspacingとalignmentに従って配置します
	class UiVerticalLayout : public UiLayout {
	public:
		UiVerticalLayout();
		~UiVerticalLayout() override;

		const char* GetTypeName() const override {
			return "VerticalLayoutPreset";
		}
	};
}
