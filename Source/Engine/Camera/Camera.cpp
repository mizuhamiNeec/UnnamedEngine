#include "Camera.h"

#include "imgui/imgui.h"
#include "Math/Matrix/Mat4.h"
#include "Structs/Structs.h"
#include <format>

#include "Utils/ClientProperties.h"

#include <algorithm>

Camera::Camera() :
	worldMat_(Mat4::Affine(transform_.scale, transform_.rotate, transform_.translate)), viewMat_(worldMat_.Inverse()),
	projMat_(Mat4::PerspectiveFovMat(vFov_, aspectRatio_, nearZ_, farZ_)),
	viewProjMat_(viewMat_* projMat_) {
}

void Camera::Update() {
#ifdef _DEBUG
	ImGui::Begin("Camera");
	ImGui::DragFloat3("pos##cam", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("rot##cam", &transform_.rotate.x, 0.01f);
	ImGui::DragFloat("vFov##cam", &vFov_, 0.01f, kFovMin, kFovMax);
	ImGui::DragFloat("nearZ", &nearZ_, 0.1f);
	ImGui::DragFloat("farZ", &farZ_, 0.1f);
	ImGui::Text(std::format("{}", aspectRatio_).c_str());
	ImGui::End();
#endif

	// transform_からアフィン変換行列を作成
	worldMat_ = Mat4::Affine(
		transform_.scale,
		transform_.rotate,
		transform_.translate
	);

	// worldMatの逆行列
	viewMat_ = worldMat_.Inverse();

	// プロジェクション行列
	projMat_ = Mat4::PerspectiveFovMat(vFov_, aspectRatio_, nearZ_, farZ_);

	// 合成行列
	viewProjMat_ = viewMat_ * projMat_;
}

void Camera::SetPos(const Vec3& newPos) {
	transform_.translate = newPos;
}

void Camera::SetRotate(const Vec3& newRot) {
	transform_.rotate = newRot;
}

void Camera::SetFovVertical(const float& newFovVertical) {
	vFov_ = std::clamp(newFovVertical, kFovMin, kFovMax);
}

void Camera::SetFovHorizontal(const float& newFovHorizontal) {
	hFov_ = newFovHorizontal;
}

void Camera::SetNearZ(const float& newNearZ) {
	nearZ_ = newNearZ;
}

void Camera::SetFarZ(const float& newFarZ) {
	farZ_ = newFarZ;
}

void Camera::SetAspectRatio(const float newAspectRatio) {
	aspectRatio_ = newAspectRatio;
}

Vec3& Camera::GetPos() {
	return transform_.translate;
}

Vec3& Camera::GetRotate() {
	return transform_.rotate;
}

float& Camera::GetFovHorizontal() {
	return hFov_;
}

float& Camera::GetFovVertical() {
	return vFov_;
}

float& Camera::GetNearZ() {
	return nearZ_;
}

float& Camera::GetFarZ() {
	return farZ_;
}

Mat4& Camera::GetViewProjMat() {
	return viewProjMat_;
}

