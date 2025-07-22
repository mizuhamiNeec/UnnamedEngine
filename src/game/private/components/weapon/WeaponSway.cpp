#ifdef _DEBUG
#include <imgui.h>
#endif

#include <engine/public/Components/Transform/SceneComponent.h>
#include <engine/public/Input/InputSystem.h>

#include <game/public/components/weapon/WeaponSway.h>

WeaponSway::~WeaponSway() = default;

void WeaponSway::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
}

void WeaponSway::Update([[maybe_unused]] const float deltaTime) {
	// マウスの移動量を取得
	Vec2 delta = InputSystem::GetMouseDelta();

	mPitch += delta.y * mSwayAmount * deltaTime;
	mYaw += delta.x * mSwayAmount * deltaTime;

	mPitch = std::lerp(mPitch, 0.0f, mAttenuation * deltaTime); // ピッチの減衰
	mYaw   = std::lerp(mYaw, 0.0f, mAttenuation * deltaTime);   // ヨーの減衰

	const Quaternion pitch = Quaternion::AxisAngle(Vec3::right, mPitch);
	const Quaternion yaw   = Quaternion::AxisAngle(Vec3::up, mYaw);

	const Quaternion finalRotation = yaw * pitch;

	mScene->SetLocalPos(Vec3(delta * 0.0001f)); // 武器の位置をマウスの移動量に基づいて調整

	// クォータニオンに変換してセット
	mScene->SetLocalRot(finalRotation);
	mScene->SetLocalPos(
		Math::Lerp(mScene->GetLocalPos(),
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
