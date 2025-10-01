#include "CameraRotator.h"

#include <engine/Entity/Entity.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConVarManager.h>

CameraRotator::~CameraRotator() {
}

void CameraRotator::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mScene = mOwner->GetTransform();

	// 初期回転を取得
	mPitch = mScene->GetLocalRot().ToEulerAngles().x;
	mYaw = mScene->GetLocalRot().ToEulerAngles().y;

	ConVarManager::RegisterConVar("m_pitch", 0.022f, "Mouse pitch factor.");
	ConVarManager::RegisterConVar("m_yaw", 0.022f, "Mouse yaw factor.");
	ConVarManager::RegisterConVar("cl_pitchup", 89.0f);
	ConVarManager::RegisterConVar("cl_pitchdown", 89.0f);
}

void CameraRotator::Update([[maybe_unused]] float deltaTime) {
	Vec2 delta = InputSystem::GetMouseDelta();

	// 感度と回転値を計算
	const float sensitivity = ConVarManager::GetConVar("sensitivity")->
		GetValueAsFloat();
	const float m_pitch = ConVarManager::GetConVar("m_pitch")->
		GetValueAsFloat();
	const float m_yaw = ConVarManager::GetConVar("m_yaw")->GetValueAsFloat();
	const float cl_pitchdown = ConVarManager::GetConVar("cl_pitchdown")->
		GetValueAsFloat();
	const float cl_pitchup = ConVarManager::GetConVar("cl_pitchup")->
		GetValueAsFloat();

	mPitch += delta.y * sensitivity * m_pitch;
	mYaw += delta.x * sensitivity * m_yaw;

	// ピッチをクランプ（上下回転の制限）
	mPitch = std::clamp(mPitch, -cl_pitchup, cl_pitchdown);

	// クォータニオンを生成（回転順序: ヨー → ピッチ）
	Quaternion yawRotation = Quaternion::AxisAngle(
		Vec3::up, mYaw * Math::deg2Rad);
	Quaternion pitchRotation = Quaternion::AxisAngle(
		Vec3::right, mPitch * Math::deg2Rad);

	// 回転を合成して設定
	Quaternion finalRotation = yawRotation * pitchRotation;
	mScene->SetLocalRot(finalRotation);
}

void CameraRotator::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("Camera Rotator",
		ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Pitch: %.2f", mPitch);
		ImGui::Text("Yaw: %.2f", mYaw);
	}
#endif
}
