#include "CameraComponent.h"

#include <Entity/Base/Entity.h>

#include <ImGuiManager/ImGuiManager.h>

#include <Camera/CameraManager.h>
#include <Lib/Utils/ClientProperties.h>

#include <Window/WindowManager.h>

CameraComponent::~CameraComponent() {}

void CameraComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	// 親からTransformComponentを取得
	transform_ = owner_->GetTransform();

	aspectRatio_ = (16.0f / 9.0f);
	worldMat_ = transform_->GetWorldMat();
	viewMat_ = worldMat_.Inverse();
	projMat_ = Mat4::PerspectiveFovMat(fov_, aspectRatio_, zNear_, zFar_);
	viewProjMat_ = viewMat_ * projMat_;
}

void CameraComponent::Update([[maybe_unused]] float deltaTime) {
	// TODO : 毎フレームアスペクト比を計算するのは無駄なので、ウィンドウサイズ変更時のみ計算するようにする
	const float width = static_cast<float>(WindowManager::GetMainWindow()->GetClientWidth());
	const float height = static_cast<float>(WindowManager::GetMainWindow()->GetClientHeight());
	SetAspectRatio(width / height);

	// 変更があった場合のみ更新
	//	if (transform_->IsDirty()) {
	const Vec3& pos = transform_->GetWorldPos();
	const Vec3& scale = transform_->GetWorldScale();
	const Quaternion& rot = transform_->GetWorldRot();

	const Mat4 S = Mat4::Scale(scale);
	const Mat4 R = Mat4::FromQuaternion(rot);
	const Mat4 T = Mat4::Translate(pos);

	worldMat_ = R * S * T;
	viewMat_ = worldMat_.Inverse();
	projMat_ = Mat4::PerspectiveFovMat(fov_, aspectRatio_, zNear_, zFar_);
	viewProjMat_ = viewMat_ * projMat_;

	transform_->SetIsDirty(false);
}

void CameraComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
		if (ImGui::Button("PrevCamera")) {
			CameraManager::SwitchToNextCamera();
		}

		ImGui::SameLine();

		if (ImGui::Button("NextCamera")) {
			CameraManager::SwitchToPrevCamera();
		}

		float fovTmp = fov_ * Math::rad2Deg;
		if (ImGui::DragFloat("FOV##cam", &fovTmp, 0.1f, kFovMin * Math::rad2Deg, kFovMax * Math::rad2Deg, "%.2f [deg]")) {
			SetFovVertical(fovTmp * Math::deg2Rad);
		}

		ImGui::DragFloat("ZNear##cam", &zNear_, 0.1f);
		ImGui::DragFloat("ZFar##cam", &zFar_, 0.1f);
		ImGui::DragFloat("AspectRatio##cam", &aspectRatio_, 0.1f);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 垂直視野角を取得します [rad]
//-----------------------------------------------------------------------------
float& CameraComponent::GetFovVertical() {
	return fov_;
}

//-----------------------------------------------------------------------------
// Purpose: 垂直視野角を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetFovVertical(const float& newFovVertical) {
	fov_ = std::clamp(newFovVertical, kFovMin, kFovMax);
}

//-----------------------------------------------------------------------------
// Purpose: ニアクリップ距離を取得します
//-----------------------------------------------------------------------------
float& CameraComponent::GetZNear() {
	return zNear_;
}

//-----------------------------------------------------------------------------
// Purpose: ニアクリップ距離を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetNearZ(const float& newNearZ) {
	zNear_ = newNearZ;
}

//-----------------------------------------------------------------------------
// Purpose: ファークリップ距離を取得します
//-----------------------------------------------------------------------------
float& CameraComponent::GetZFar() {
	return zFar_;
}

//-----------------------------------------------------------------------------
// Purpose: ファークリップ距離を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetFarZ(const float& newFarZ) {
	zFar_ = newFarZ;
}

//-----------------------------------------------------------------------------
// Purpose: ビュープロジェクション行列を取得します
//-----------------------------------------------------------------------------
Mat4& CameraComponent::GetViewProjMat() {
	return viewProjMat_;
}

//-----------------------------------------------------------------------------
// Purpose: ビュー行列を取得します
//-----------------------------------------------------------------------------
Mat4& CameraComponent::GetViewMat() {
	return viewMat_;
}

//-----------------------------------------------------------------------------
// Purpose: プロジェクション行列を取得します
//-----------------------------------------------------------------------------
Mat4& CameraComponent::GetProjMat() {
	return projMat_;
}

float& CameraComponent::GetAspectRatio() {
	return aspectRatio_;
}

//-----------------------------------------------------------------------------
// Purpose: アスペクト比を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetAspectRatio(const float newAspectRatio) {
	aspectRatio_ = newAspectRatio;
}

