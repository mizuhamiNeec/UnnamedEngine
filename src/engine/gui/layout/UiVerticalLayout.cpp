#include "UiVerticalLayout.h"

namespace Unnamed::Gui {
	UiVerticalLayout::UiVerticalLayout() {
		(void)GetOrAddComponent<UiVerticalLayoutComponent>();
		SyncLayoutComponent();
	}

	UiVerticalLayout::~UiVerticalLayout() = default;
}
