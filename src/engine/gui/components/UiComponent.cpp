#include "UiComponent.h"

#include "engine/gui/UiDrawCommand.h"

namespace Unnamed::Gui {
	void UiComponent::OnAttached(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::OnDetached(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::OnTick(UiWidget& owner, const float deltaTime) {
		(void)owner;
		(void)deltaTime;
	}

	void UiComponent::OnBeforeLayout(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::OnAfterLayout(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::BuildDrawCommands(
		const UiWidget&             owner,
		std::vector<UiDrawCommand>& out
	) const {
		(void)owner;
		(void)out;
	}

	void UiComponent::OnMouseEnter(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::OnMouseLeave(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::OnMouseDown(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::OnMouseUp(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::OnClick(UiWidget& owner) {
		(void)owner;
	}

	void UiComponent::Serialize(JsonWriter& writer) const {
		(void)writer;
	}

	void UiComponent::Deserialize(const JsonReader& reader) {
		(void)reader;
	}
}
