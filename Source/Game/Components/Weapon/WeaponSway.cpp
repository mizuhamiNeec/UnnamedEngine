#include "WeaponSway.h"

#include "imgui.h"
#include "Entity/Base/Entity.h"
#include "Input/InputSystem.h"
#include "Lib/Math/MathLib.h"

WeaponSway::~WeaponSway() {
}

void WeaponSway::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
}

void WeaponSway::Update([[maybe_unused]] const float deltaTime) {
	// マウスの移動量を取得
	Vec2 delta = InputSystem::GetMouseDelta();

	TransformComponent* transform = owner_->GetTransform();

	mPitch += delta.y * mSwayAmount * deltaTime;
	mYaw += delta.x * mSwayAmount * deltaTime;

	mPitch = std::lerp(mPitch, 0.0f, mAttenuation * deltaTime); // ピッチの減衰
	mYaw   = std::lerp(mYaw, 0.0f, mAttenuation * deltaTime);   // ヨーの減衰

	Quaternion pitch = Quaternion::AxisAngle(Vec3::right, mPitch);
	Quaternion yaw   = Quaternion::AxisAngle(Vec3::up, mYaw);

	Quaternion finalRotation = yaw * pitch;

	transform->SetLocalPos(delta * 0.0001f); // 武器の位置をマウスの移動量に基づいて調整

	// クォータニオンに変換してセット
	transform->SetLocalRot(finalRotation);
	transform->SetLocalPos(
		Math::Lerp(transform->GetLocalPos(),
		           Vec3::zero,
		           (mAttenuation * 0.5f) * deltaTime)); // 武器の位置調整
}

void WeaponSway::Render(ID3D12GraphicsCommandList* commandList) {
	Component::Render(commandList);
}

void WeaponSway::DrawInspectorImGui() {
	if (ImGui::CollapsingHeader(
		"Weapon Sway", ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Pitch: %.2f, Yaw: %.2f", mPitch, mYaw);

		ImGui::DragFloat("SwayAmount", &mSwayAmount, 0.01f);
		ImGui::DragFloat("Attenuation", &mAttenuation, 0.01f, 0.0f, 10.0f);
	}
}

Entity* WeaponSway::GetOwner() const {
	return Component::GetOwner();
}
