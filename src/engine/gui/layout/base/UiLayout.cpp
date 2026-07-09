#include "UiLayout.h"

namespace Unnamed::Gui {
	UiLayout::UiLayout()  = default;
	UiLayout::~UiLayout() = default;

	void UiLayout::SetPadding(const LayoutPadding& padding) {
		mPadding = padding;
		SyncLayoutComponent();
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	const LayoutPadding& UiLayout::GetPadding() const {
		return mPadding;
	}

	void UiLayout::SetSpacing(const float spacing) {
		mSpacing = spacing;
		SyncLayoutComponent();
		MarkDirty(DIRTY_FLAGS::LAYOUT | DIRTY_FLAGS::DRAW);
	}

	float UiLayout::GetSpacing() const {
		return mSpacing;
	}

	void UiLayout::SyncLayoutComponent() const {
		auto* self = const_cast<UiLayout*>(this);
		if (auto* vertical = self->GetComponent<UiVerticalLayoutComponent>()) {
			vertical->SetPadding(mPadding);
			vertical->SetSpacing(mSpacing);
		}
		if (auto* horizontal = self->GetComponent<
			UiHorizontalLayoutComponent>()) {
			horizontal->SetPadding(mPadding);
			horizontal->SetSpacing(mSpacing);
		}
	}
}
