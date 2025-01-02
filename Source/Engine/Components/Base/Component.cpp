#include "Component.h"

Component::~Component() = default;

void Component::OnAttach(Entity& owner) { this->owner_ = &owner; }
