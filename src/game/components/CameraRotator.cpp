#include "CameraRotator.h"

#include <engine/Entity/Entity.h>
#include <engine/Input/InputSystem.h>
#include <engine/OldConsole/ConVarManager.h>

/**
 * @brief デストラクタ
 */
CameraRotator::~CameraRotator() {}

void CameraRotator::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	mScene = mOwner->GetTransform();

	// 初期回転を取得
	mPitch = mScene->GetLocalRot().ToEulerAngles().x;
	mYaw   = mScene->GetLocalRot().ToEulerAngles().y;

	ConVarManager::RegisterConVar("m_pitch", 0.022f, "Mouse pitch factor.");
	ConVarManager::RegisterConVar("m_yaw", 0.022f, "Mouse yaw factor.");
	ConVarManager::RegisterConVar("cl_pitchup", 89.0f);
	ConVarManager::RegisterConVar("cl_pitchdown", 89.0f);

	// スティック設定
	ConVarManager::RegisterConVar(
		"joy_response_curve", 10.0f, "Stick response curve."
	);
	ConVarManager::RegisterConVar(
		"joy_outer_threshold", 0.90f, "Stick outer threshold."
	);
	ConVarManager::RegisterConVar(
		"joy_turning_extra_yaw", 0.0f, "Extra yaw speed."
	);
	ConVarManager::RegisterConVar(
		"joy_turning_extra_pitch", 0.0f, "Extra pitch speed."
	);
}

void CameraRotator::PrePhysics(float) {
	const Vec2 delta = InputSystem::GetMouseDelta();

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

	// スティック設定の取得
	const float joy_curve = ConVarManager::GetConVar("joy_response_curve")->
		GetValueAsFloat();
	const float joy_threshold = ConVarManager::GetConVar("joy_outer_threshold")
		->
		GetValueAsFloat();
	const float joy_extra_yaw = ConVarManager::GetConVar(
			"joy_turning_extra_yaw"
		)->
		GetValueAsFloat();
	const float joy_extra_pitch = ConVarManager::GetConVar(
		"joy_turning_extra_pitch"
	)->GetValueAsFloat();

	// マウス入力の適用
	mPitch += delta.y * sensitivity * m_pitch;
	mYaw   += delta.x * sensitivity * m_yaw;

	// スティック入力の取得
	Vec2  rightStick = InputSystem::GetRightStick(0);
	float stickMag   = rightStick.Length();

	if (stickMag > 0.0f) {
		// マグニチュードをクリップ
		stickMag        = std::min(stickMag, 1.0f);
		float exponent  = 1.0f + (joy_curve / 10.0f);
		float curvedMag = std::pow(stickMag, exponent);

		Vec2 stickDir    = rightStick.Normalized();
		Vec2 curvedStick = stickDir * curvedMag;

		float extraYaw   = 0.0f;
		float extraPitch = 0.0f;
		if (stickMag >= joy_threshold) {
			extraYaw   = joy_extra_yaw * std::abs(stickDir.x);
			extraPitch = joy_extra_pitch * std::abs(stickDir.y);
		}

		float finalPitchInput = (curvedStick.y * sensitivity * m_pitch * 50.0f)
		                        + (extraPitch * std::copysign(
			                           1.0f, curvedStick.y
		                           ) * 120.0f);
		float finalYawInput = (curvedStick.x * sensitivity * m_yaw * 50.0f) + (
			                      extraYaw * std::copysign(
				                      1.0f, curvedStick.x
			                      ) * 120.0f);

		mPitch -= finalPitchInput;
		mYaw   += finalYawInput;
	}

	// ピッチをクランプ（上下回転の制限）
	mPitch = std::clamp(mPitch, -cl_pitchup, cl_pitchdown);

	// クォータニオンを生成（回転順序: ヨー → ピッチ → ロール）
	Quaternion yawRotation = Quaternion::AxisAngle(
		Vec3::up, mYaw * Math::deg2Rad
	);
	Quaternion pitchRotation = Quaternion::AxisAngle(
		Vec3::right, (mPitch + mAnimationPitchOffset) * Math::deg2Rad
	);
	Quaternion rollRotation = Quaternion::AxisAngle(
		Vec3::forward, mAnimationRollOffset * Math::deg2Rad
	);

	// 回転を合成して設定（アニメーションオフセットを含む）
	Quaternion finalRotation = yawRotation * pitchRotation * rollRotation;
	mScene->SetLocalRot(finalRotation);
}

void CameraRotator::Update(float) {}

/**
 * @brief ImGuiインスペクタでの表示処理
 * @details デバッグモードで現在のピッチとヨーの値を表示します
 */
void CameraRotator::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader(
		"Camera Rotator",
		ImGuiTreeNodeFlags_DefaultOpen
	)) {
		ImGui::Text("Pitch: %.2f", mPitch);
		ImGui::Text("Yaw: %.2f", mYaw);
	}
#endif
}
