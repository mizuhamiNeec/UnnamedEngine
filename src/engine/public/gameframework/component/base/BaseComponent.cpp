#include <engine/public/gameframework/component/base/BaseComponent.h>

namespace Unnamed {
	BaseComponent::BaseComponent(
		std::string name
	): mName(std::move(name)) {
	}

	void BaseComponent::OnEditorTick(float) {
	}

	void BaseComponent::OnEditorRender() const {
	}

	std::string_view BaseComponent::GetName() {
		return mName;
	}
}
