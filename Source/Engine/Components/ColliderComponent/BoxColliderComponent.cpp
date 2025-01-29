#include "BoxColliderComponent.h"

#include <Components/Transform/TransformComponent.h>

#include "Debug/Debug.h"

#include "Entity/Base/Entity.h"

#include "Physics/Physics.h"

BoxColliderComponent::BoxColliderComponent() = default;

void BoxColliderComponent::OnAttach(Entity& owner) {
	ColliderComponent::OnAttach(owner);
	transform_ = owner.GetTransform();
}

void BoxColliderComponent::Update([[maybe_unused]] float deltaTime) {
	Debug::DrawBox(transform_->GetWorldPos(), Quaternion::identity, size_, { 0.66f, 0.8f, 0.85f, 1.0f });
}

void BoxColliderComponent::DrawInspectorImGui() {
	if (ImGui::CollapsingHeader("BoxColliderComponent", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Size", &size_.x, 0.1f);
	}
}

bool BoxColliderComponent::CheckCollision([[maybe_unused]] const ColliderComponent* other) const {
	return false;
}

bool BoxColliderComponent::IsDynamic() {
	return true;
}

void BoxColliderComponent::SetSize(const Vec3& size) {
	size_ = size;
}

const Vec3& BoxColliderComponent::GetSize() const {
	return size_;
}

AABB BoxColliderComponent::GetBoundingBox() const {
	Vec3 halfSize = size_ * 0.5f;
	Vec3 min = transform_->GetWorldPos() - halfSize;
	Vec3 max = transform_->GetWorldPos() + halfSize;
	return AABB(min, max);
}
