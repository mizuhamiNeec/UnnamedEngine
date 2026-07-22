#include "UiHorizontalLayout.h"

namespace Unnamed::Gui {
	UiHorizontalLayout::UiHorizontalLayout() {
		(void)GetOrAddComponent<UiHorizontalLayoutComponent>();
		SyncLayoutComponent();
	}

	UiHorizontalLayout::~UiHorizontalLayout() = default;
}
