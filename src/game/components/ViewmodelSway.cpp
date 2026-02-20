#include "ViewmodelSway.h"

#ifdef _DEBUG
#include <imgui.h>
#endif

#include <engine/Components/Transform/SceneComponent.h>
#include <engine/Input/InputSystem.h>

ViewmodelSway::~ViewmodelSway() = default;

void ViewmodelSway::OnAttach(Entity& owner) { Component::OnAttach(owner); }

void ViewmodelSway::PrePhysics(float deltaTime) {
	// マウスの移動量を取得
	Vec2 delta = InputSystem::GetMouseDelta();

	mPitch += delta.y * mSwayAmount * deltaTime;
	mYaw   += delta.x * mSwayAmount * deltaTime;

	mPitch = std::lerp(mPitch, 0.0f, mAttenuation * deltaTime); // ピッチの減衰
	mYaw   = std::lerp(mYaw, 0.0f, mAttenuation * deltaTime);   // ヨーの減衰

	const Quaternion pitch = Quaternion::AxisAngle(Vec3::right, mPitch);
	const Quaternion yaw   = Quaternion::AxisAngle(Vec3::up, mYaw);

	const Quaternion finalRotation = yaw * pitch;

	mScene->SetLocalPos(Vec3(delta * 0.0001f)); // 武器の位置をマウスの移動量に基づいて調整

	// クォータニオンに変換してセット
	mScene->SetLocalRot(finalRotation);
	mScene->SetLocalPos(
		Math::Lerp(
			mScene->GetLocalPos(),
			Vec3::zero,
			(mAttenuation * 0.5f) * deltaTime
		)
	); // 武器の位置調整
}

void ViewmodelSway::Update(const float) {}

void ViewmodelSway::Render(ID3D12GraphicsCommandList*) {}

void ViewmodelSway::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader(
		"Viewmodel Sway", ImGuiTreeNodeFlags_DefaultOpen
	)) {
		ImGui::Text("Pitch: %.2f, Yaw: %.2f", mPitch, mYaw);

		ImGui::DragFloat("SwayAmount", &mSwayAmount, 0.01f);
		ImGui::DragFloat("Attenuation", &mAttenuation, 0.01f, 0.0f, 10.0f);
	}
#endif
}

Entity* ViewmodelSway::GetOwner() const { return Component::GetOwner(); }
