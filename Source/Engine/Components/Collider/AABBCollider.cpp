#include "AABBCollider.h"

#include "Debug/Debug.h"

#include "Entity/Base/Entity.h"

#include "ImGuiManager/ImGuiManager.h"

AABBCollider::AABBCollider(const Vec3& size) : aabb_({ Vec3::zero, Vec3::zero }), size_(size) {
	colliderType_ = ColliderType::AABB;
}

void AABBCollider::Update([[maybe_unused]] float deltaTime) {
	aabb_.min = owner_->GetTransform()->GetWorldPos() - size_ * 0.5f;
	aabb_.max = owner_->GetTransform()->GetWorldPos() + size_ * 0.5f;

	Debug::DrawBox(owner_->GetTransform()->GetWorldPos(), Quaternion::identity, size_, Vec4::green);
}

void AABBCollider::DrawInspectorImGui() {
	ImGui::CollapsingHeader("AABBCollider", ImGuiTreeNodeFlags_DefaultOpen);
	ImGuiManager::DragVec3("Size", size_, 0.1f, "%.2f");
}

ColliderType AABBCollider::GetColliderType() {
	return colliderType_;
}

void AABBCollider::OnAttach(Entity& owner) {
	Collider::OnAttach(owner);
}

Physics::AABB AABBCollider::GetAABB() const {
	return aabb_;
}
