#include "Component.h"

Component::Component() = default;

Component::~Component() {
}

void Component::OnAttach(Entity& owner) {
	this->owner_ = &owner;
}

void Component::Render(ID3D12GraphicsCommandList* commandList) {
	commandList;
}

bool Component::IsEditorOnly() const {
	return false;
}
