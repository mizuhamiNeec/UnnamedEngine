#include "CameraComponent.h"

#include <algorithm>

#include "../ImGuiManager/ImGuiManager.h"
#include "../Lib/Math/Matrix/Mat4.h"
#include "../Lib/Utils/ClientProperties.h"
#include "../EntityComponentSystem/Components/Transform/TransformComponent.h"
#include "../EntityComponentSystem/Entity/Base/BaseEntity.h"

CameraComponent::CameraComponent(BaseEntity* owner) : BaseComponent(owner) {}

void CameraComponent::Initialize() {
	UpdateViewProjMat();
}

void CameraComponent::Update([[maybe_unused]] float deltaTime) {
	UpdateViewProjMat();

#ifdef _DEBUG
	ImGui::Begin("CameraComponent");
	if (ImGui::CollapsingHeader("Properties")) {
		float fovTmp = fov_ * Math::rad2Deg;
		if (ImGui::DragFloat("FOV##cam", &fovTmp, 0.1f, kFovMin * Math::rad2Deg, kFovMax * Math::rad2Deg, "%.2f [deg]")) {
			SetFovVertical(fovTmp * Math::deg2Rad);
		}
		ImGui::DragFloat("ZNear##cam", &zNear_, 0.1f);
		ImGui::DragFloat("ZFar##cam", &zFar_, 0.1f);
		ImGui::BeginDisabled();
		ImGui::DragFloat("AspectRatio##cam", &aspectRatio_, 0.1f);
		ImGui::EndDisabled();
	}
	ImGui::End();
#endif
}

void CameraComponent::Terminate() {

}

void CameraComponent::Serialize([[maybe_unused]] std::ostream& out) const {
}

void CameraComponent::Deserialize([[maybe_unused]] std::istream& in) {
}

void CameraComponent::ImGuiDraw() {
}

//-----------------------------------------------------------------------------
// Purpose: 垂直視野角を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetFovVertical(const float& newFovVertical) {
	fov_ = std::clamp(newFovVertical, kFovMin, kFovMax);
}

//-----------------------------------------------------------------------------
// Purpose: ニアクリップ距離を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetNearZ(const float& newNearZ) {
	zNear_ = newNearZ;
}

//-----------------------------------------------------------------------------
// Purpose: ファークリップ距離を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetFarZ(const float& newFarZ) {
	zFar_ = newFarZ;
}

//-----------------------------------------------------------------------------
// Purpose: アスペクト比を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetAspectRatio(const float newAspectRatio) {
	aspectRatio_ = newAspectRatio;
}

//-----------------------------------------------------------------------------
// Purpose: 垂直視野角を取得します [rad]
//-----------------------------------------------------------------------------
float& CameraComponent::GetFovVertical() {
	return fov_;
}

//-----------------------------------------------------------------------------
// Purpose: ニアクリップ距離を取得します
//-----------------------------------------------------------------------------
float& CameraComponent::GetZNear() {
	return zNear_;
}

//-----------------------------------------------------------------------------
// Purpose: ファークリップ距離を取得します
//-----------------------------------------------------------------------------
float& CameraComponent::GetZFar() {
	return zFar_;
}

float& CameraComponent::GetAspectRatio() {
	return aspectRatio_;
}

//-----------------------------------------------------------------------------
// Purpose: ビュープロジェクション行列を取得します
//-----------------------------------------------------------------------------
Mat4& CameraComponent::GetViewProjMat() {
	return viewProjMat_;
}

Mat4 CameraComponent::GetViewMat() const {
	auto* transform = ParentEntity()->GetComponent<TransformComponent>();
	if (!transform) return Mat4::identity;

	const Vec3 pos = transform->GetWorldPosition();
	const Quaternion rot = transform->GetWorldRotation();

	return Mat4::Affine(
		Vec3::one,
		rot.ToEulerAngles(),
		pos
	).Inverse();
}

Mat4 CameraComponent::GetProjMat() const {
	return Mat4::PerspectiveFovMat(fov_, aspectRatio_, zNear_, zFar_);
}

void CameraComponent::UpdateViewProjMat() {
	auto* transform = ParentEntity()->GetComponent<TransformComponent>();

	if (!transform) return;

	const Vec3 pos = transform->GetWorldPosition();
	const Quaternion rot = transform->GetWorldRotation();

	Mat4 viewMat = Mat4::Affine(
		Vec3::one,
		rot.ToEulerAngles(),
		pos
	).Inverse();

	Mat4 projMat = GetProjMat();

	viewProjMat_ = viewMat * projMat;
}
