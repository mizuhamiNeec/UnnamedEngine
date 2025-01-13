#include "ColliderComponent.h"

#include "Entity/Base/Entity.h"

ColliderComponent::~ColliderComponent() {
}

void ColliderComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	transform_ = owner_->GetTransform();
}

void ColliderComponent::Update([[maybe_unused]] float deltaTime) {
}

void ColliderComponent::DrawInspectorImGui() {
}

float ColliderComponent::GetRadius() const {
	return radius_;
}

Vec3 ColliderComponent::GetPosition() const {
	return transform_->GetWorldPos();
}

void ColliderComponent::RegisterCollisionCallback(CollisionCallback callback) {
	collisionCallbacks_.emplace_back(std::move(callback));
}

void ColliderComponent::OnCollision(Entity* other) const {
	for (const auto& callback : collisionCallbacks_) {
		callback(other);
	}
}
