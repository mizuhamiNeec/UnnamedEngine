#include "CameraRotator.h"

#include <Entity/Base/Entity.h>

#include <Input/InputSystem.h>

#include <SubSystem/Console/ConVarManager.h>
#include <Lib/Math/Vector/Vec3.h>
#include <Lib/Math/MathLib.h>

CameraRotator::~CameraRotator() {
}

void CameraRotator::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	transform_ = mOwner->GetTransform();

	// 初期回転を取得
	pitch_ = transform_->GetLocalRot().ToEulerAngles().x;
	yaw_   = transform_->GetLocalRot().ToEulerAngles().y;

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

	pitch_ += delta.y * sensitivity * m_pitch;
	yaw_ += delta.x * sensitivity * m_yaw;

	// ピッチをクランプ（上下回転の制限）
	pitch_ = std::clamp(pitch_, -cl_pitchup, cl_pitchdown);

	// クォータニオンを生成（回転順序: ヨー → ピッチ）
	Quaternion yawRotation = Quaternion::AxisAngle(
		Vec3::up, yaw_ * Math::deg2Rad);
	Quaternion pitchRotation = Quaternion::AxisAngle(
		Vec3::right, pitch_ * Math::deg2Rad);

	// 回転を合成して設定
	Quaternion finalRotation = yawRotation * pitchRotation;
	transform_->SetLocalRot(finalRotation);
}

void CameraRotator::DrawInspectorImGui() {
	if (ImGui::CollapsingHeader("Camera Rotator",
	                            ImGuiTreeNodeFlags_DefaultOpen)) {
		ImGui::Text("Pitch: %.2f", pitch_);
		ImGui::Text("Yaw: %.2f", yaw_);
	}
}
