#include <engine/Components/ColliderComponent/BoxColliderComponent.h>
#include <engine/Debug/Debug.h>
#include <engine/Entity/Entity.h>

#ifdef _DEBUG
#include <imgui.h>
#endif

/// @brief コンストラクタ
BoxColliderComponent::BoxColliderComponent() = default;

/// @brief エンティティにアタッチされたときの処理
/// @param owner 所有エンティティ
void BoxColliderComponent::OnAttach(Entity& owner) {
	ColliderComponent::OnAttach(owner);
	transform_ = owner.GetTransform();
}

/// @brief 更新処理
/// @param deltaTime デルタタイム
void BoxColliderComponent::Update([[maybe_unused]] float deltaTime) {
	Debug::DrawBox(transform_->GetWorldPos() + offset_, Quaternion::identity,
	               size_, {0.66f, 0.8f, 0.85f, 1.0f});
}

/// @brief ImGuiによるインスペクター描画
void BoxColliderComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("BoxColliderComponent",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::DragFloat3("Size", &size_.x, 0.1f);
		ImGui::DragFloat3("Offset", &offset_.x, 0.1f);
	}
#endif
}

/// @brief 他のコライダーとの衝突判定
/// @param other 他のコライダーコンポーネント
/// @return 衝突しているかどうか
bool BoxColliderComponent::CheckCollision(
	[[maybe_unused]] const ColliderComponent* other) const {
	return false;
}

/// @brief 動的コライダーかどうかを取得
/// @return 動的コライダーであればtrue、静的コライダーであればfalse
bool BoxColliderComponent::IsDynamic() {
	return true;
}

/// @brief コライダーのサイズを設定
/// @param size コライダーのサイズ
void BoxColliderComponent::SetSize(const Vec3& size) {
	size_ = size;
}

/// @brief コライダーのサイズを取得
/// @return コライダーのサイズ
const Vec3& BoxColliderComponent::GetSize() const {
	return size_;
}
