#include "UiButton.h"

#include "components/UiButtonBehaviorComponent.h"

namespace Unnamed::Gui {
	UiButton::UiButton() {
		(void)GetOrAddComponent<UiButtonBehaviorComponent>();
	}

	void UiButton::SetText(const std::string_view& text) const {
		if (auto* behavior = GetBehavior()) {
			behavior->SetText(text);
		}
	}

	std::string_view UiButton::GetText() const {
		if (const auto* behavior = GetBehavior()) {
			return behavior->GetText();
		}
		return {};
	}

	void UiButton::SetOnClick(const std::function<void()>& callback) const {
		if (auto* behavior = GetBehavior()) {
			behavior->SetOnClick(callback);
		}
	}

	void UiButton::SetColors(
		const Color& normal,
		const Color& hovered,
		const Color& pressed
	) const {
		if (auto* behavior = GetBehavior()) {
			behavior->SetColors(normal, hovered, pressed);
		}
	}

	UiButtonBehaviorComponent* UiButton::GetBehavior() const {
		return const_cast<UiButton*>(this)->GetOrAddComponent<
			UiButtonBehaviorComponent>();
	}
}
