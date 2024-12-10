#include "Camera.h"

#include "../../Components/Transform/TransformComponent.h"
#include "../../Components/Camera/CameraComponent.h"

Camera::Camera() : BaseEntity() {
	// コンポーネントを追加
	transform_ = AddComponent<TransformComponent>();
	camera_ = AddComponent<CameraComponent>();
}

void Camera::SetWorldPos(const Vec3& newWorldPos) const {
	transform_->SetWorldPos(newWorldPos);
}

void Camera::SetWorldRot(const Quaternion& newWorldRot) const {
	transform_->SetWorldRot(newWorldRot);
}

void Camera::LookAt(const Vec3& target, const Vec3& up) {
	target;
	up;
}

void Camera::SetFov(const float& fovDegrees) const {
	camera_->SetFovVertical(fovDegrees * Math::deg2Rad);
}

void Camera::SetNearZ(const float& newNearZ) const {
	camera_->SetNearZ(newNearZ);
}

void Camera::SetFarZ(const float& newFarZ) const {
	camera_->SetFarZ(newFarZ);
}

void Camera::SetAspectRatio(const float& newAspectRatio) const {
	camera_->SetAspectRatio(newAspectRatio);
}

Vec3 Camera::GetWorldPos() const {
	return transform_->GetWorldPosition();
}

Quaternion Camera::GetRot() const {
	return transform_->GetWorldRotation();
}

float Camera::GetFov() const {
	return camera_->GetFovVertical();
}

float Camera::GetNearZ() const {
	return camera_->GetZNear();
}

float Camera::GetFarZ() const {
	return camera_->GetZFar();
}

float Camera::GetAspectRatio() const {
	return camera_->GetAspectRatio();
}

Mat4 Camera::GetViewMat() const {
	return camera_->GetViewMat();
}

Mat4 Camera::GetProjMat() const {
	return camera_->GetProjMat();
}

Mat4 Camera::GetViewProjMat() const {
	return camera_->GetViewProjMat();
}

TransformComponent* Camera::GetTransform() const {
	return transform_;
}

CameraComponent* Camera::GetCamera() const {
	return camera_;
}
