#include "Camera.h"

#include <algorithm>
#include <format>

#include "../ImGuiManager/ImGuiManager.h"

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

#include "../Lib/Math/Matrix/Mat4.h"

#include "../Lib/Structs/Structs.h"

#include "../Lib/Utils/ClientProperties.h"

Camera::Camera() :
	worldMat_(Mat4::Affine(transform_.scale, transform_.rotate, transform_.translate)), viewMat_(worldMat_.Inverse()),
	aspectRatio_(16.0f / 9.0f),
	projMat_(Mat4::PerspectiveFovMat(fov_, aspectRatio_, zNear_, zFar_)),
	viewProjMat_(viewMat_* projMat_) {
}

void Camera::Update() {
#ifdef _DEBUG
	ImGui::Begin("Camera");
	EditTransform("Camera", transform_, 0.01f);

	if (ImGui::CollapsingHeader("Properties")) {
		float fovTmp = fov_ * Math::rad2Deg;
		if (ImGui::DragFloat("FOV##cam", &fovTmp, 0.1f, kFovMin * Math::rad2Deg, kFovMax * Math::rad2Deg,
			"%.2f [deg]")) {
			SetFovVertical(fovTmp * Math::deg2Rad);
		}

		ImGui::DragFloat("ZNear##cam", &zNear_, 0.1f);
		ImGui::DragFloat("ZFar##cam", &zFar_, 0.1f);
		ImGui::DragFloat("AspectRatio##cam", &aspectRatio_, 0.1f);
	}
	ImGui::End();
#endif

	// transform_からアフィン変換行列を作成
	worldMat_ = Mat4::Affine(
		{ 1.0f, 1.0f, 1.0f },
		transform_.rotate,
		transform_.translate
	);

	// worldMatの逆行列
	viewMat_ = worldMat_.Inverse();

	// プロジェクション行列
	projMat_ = Mat4::PerspectiveFovMat(fov_, aspectRatio_, zNear_, zFar_);

	// 合成行列
	viewProjMat_ = viewMat_ * projMat_;
}

//-----------------------------------------------------------------------------
// Purpose: カメラのトランスフォームを設定します
//-----------------------------------------------------------------------------
void Camera::SetTransform(const Transform& newTransform) {
	transform_ = newTransform;
}

//-----------------------------------------------------------------------------
// Purpose: カメラの座標を設定します
//-----------------------------------------------------------------------------
void Camera::SetPos(const Vec3& newPos) {
	transform_.translate = newPos;
}

//-----------------------------------------------------------------------------
// Purpose: カメラの回転を設定します [rad]
//-----------------------------------------------------------------------------
void Camera::SetRotate(const Vec3& newRot) {
	transform_.rotate = newRot;
}

//-----------------------------------------------------------------------------
// Purpose: 垂直視野角を設定します
//-----------------------------------------------------------------------------
void Camera::SetFovVertical(const float& newFovVertical) {
	fov_ = std::clamp(newFovVertical, kFovMin, kFovMax);
}

//-----------------------------------------------------------------------------
// Purpose: ニアクリップ距離を設定します
//-----------------------------------------------------------------------------
void Camera::SetNearZ(const float& newNearZ) {
	zNear_ = newNearZ;
}

//-----------------------------------------------------------------------------
// Purpose: ファークリップ距離を設定します
//-----------------------------------------------------------------------------
void Camera::SetFarZ(const float& newFarZ) {
	zFar_ = newFarZ;
}

//-----------------------------------------------------------------------------
// Purpose: アスペクト比を設定します
//-----------------------------------------------------------------------------
void Camera::SetAspectRatio(const float newAspectRatio) {
	aspectRatio_ = newAspectRatio;
}

//-----------------------------------------------------------------------------
// Purpose: トランスフォームを取得します
//-----------------------------------------------------------------------------
Transform& Camera::GetTransform() {
	return transform_;
}

//-----------------------------------------------------------------------------
// Purpose: 座標を取得します
//-----------------------------------------------------------------------------
Vec3& Camera::GetPos() {
	return transform_.translate;
}

//-----------------------------------------------------------------------------
// Purpose: 回転を取得します [rad]
//-----------------------------------------------------------------------------
Vec3& Camera::GetRotate() {
	return transform_.rotate;
}

//-----------------------------------------------------------------------------
// Purpose: 垂直視野角を取得します [rad]
//-----------------------------------------------------------------------------
float& Camera::GetFovVertical() {
	return fov_;
}

//-----------------------------------------------------------------------------
// Purpose: ニアクリップ距離を取得します
//-----------------------------------------------------------------------------
float& Camera::GetZNear() {
	return zNear_;
}

//-----------------------------------------------------------------------------
// Purpose: ファークリップ距離を取得します
//-----------------------------------------------------------------------------
float& Camera::GetZFar() {
	return zFar_;
}

//-----------------------------------------------------------------------------
// Purpose: ビュープロジェクション行列を取得します
//-----------------------------------------------------------------------------
Mat4& Camera::GetViewProjMat() {
	return viewProjMat_;
}
