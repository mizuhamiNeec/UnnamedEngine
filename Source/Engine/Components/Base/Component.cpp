#include "Component.h"

Component::Component() = default;

Component::~Component() {
}

void Component::OnAttach(Entity& owner) {
	this->owner_ = &owner;
}

bool Component::IsEditorOnly() const {
	return false;
}
