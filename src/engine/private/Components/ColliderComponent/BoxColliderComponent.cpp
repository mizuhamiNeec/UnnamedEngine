#include <engine/public/Components/ColliderComponent/BoxColliderComponent.h>
#include <engine/public/Debug/Debug.h>
#include <engine/public/Entity/Entity.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

BoxColliderComponent::BoxColliderComponent() = default;

void BoxColliderComponent::OnAttach(Entity& owner) {
	ColliderComponent::OnAttach(owner);
	transform_ = owner.GetTransform();
}

void BoxColliderComponent::Update([[maybe_unused]] float deltaTime) {
	Debug::DrawBox(transform_->GetWorldPos() + offset_, Quaternion::identity,
	               size_, {0.66f, 0.8f, 0.85f, 1.0f});
}

void BoxColliderComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("BoxColliderComponent",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Size", &size_.x, 0.1f);
		ImGui::DragFloat3("Offset", &offset_.x, 0.1f);
	}
#endif
}

bool BoxColliderComponent::CheckCollision(
	[[maybe_unused]] const ColliderComponent* other) const {
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