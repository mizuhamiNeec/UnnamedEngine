#include "UiPanel.h"

#include "components/UiPanelStyleComponent.h"

namespace Unnamed::Gui {
	UiPanel::UiPanel() {
		(void)GetOrAddComponent<UiPanelStyleComponent>();
	}

	UiPanel::~UiPanel() = default;

	void UiPanel::SetBackgroundColor(const Color& color) const {
		if (auto* style = GetStyleComponent()) {
			style->SetBackgroundColor(color);
		}
	}

	const Color& UiPanel::GetBackgroundColor() const {
		static constexpr Color fallback = {
			.r = 0.15f, .g = 0.15f, .b = 0.18f, .a = 1.0f
		};
		if (const auto* style = GetStyleComponent()) {
			return style->GetBackgroundColor();
		}
		return fallback;
	}

	void UiPanel::SetCornerRadius(const float radius) const {
		if (auto* style = GetStyleComponent()) {
			style->SetCornerRadius(radius);
		}
	}

	float UiPanel::GetCornerRadius() const {
		if (const auto* style = GetStyleComponent()) {
			return style->GetCornerRadius();
		}
		return 4.0f;
	}

	const char* UiPanel::GetTypeName() const {
		return "PanelPreset";
	}

	UiPanelStyleComponent* UiPanel::GetStyleComponent() const {
		return const_cast<UiPanel*>(this)->GetOrAddComponent<
			UiPanelStyleComponent>();
	}
}
