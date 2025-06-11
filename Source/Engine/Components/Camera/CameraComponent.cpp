#include "CameraComponent.h"

#include <Entity/Base/Entity.h>

#include <ImGuiManager/ImGuiManager.h>

#include <Camera/CameraManager.h>
#include <Lib/Utils/ClientProperties.h>

#include <Debug/Debug.h>

CameraComponent::~CameraComponent() {
}

void CameraComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	// 親からTransformComponentを取得
	transform_ = owner_->GetTransform();

	aspectRatio_ = (16.0f / 9.0f);
	worldMat_    = transform_->GetWorldMat();
	viewMat_     = worldMat_.Inverse();
	projMat_     = Mat4::PerspectiveFovMat(fov_, aspectRatio_, zNear_, zFar_);
	viewProjMat_ = viewMat_ * projMat_;
}

void CameraComponent::Update([[maybe_unused]] float deltaTime) {
	// 変更があった場合のみ更新
	//	if (transform_->IsDirty()) {
	const Vec3&       pos   = transform_->GetWorldPos();
	const Vec3&       scale = transform_->GetWorldScale();
	const Quaternion& rot   = transform_->GetWorldRot();

	const Mat4 S = Mat4::Scale(scale);
	const Mat4 R = Mat4::FromQuaternion(rot);
	const Mat4 T = Mat4::Translate(pos);

	worldMat_    = R * S * T;
	viewMat_     = worldMat_.Inverse();
	projMat_     = Mat4::PerspectiveFovMat(fov_, aspectRatio_, zNear_, zFar_);
	viewProjMat_ = viewMat_ * projMat_;

	transform_->SetIsDirty(false);
}

void CameraComponent::Render(
	[[maybe_unused]] ID3D12GraphicsCommandList* commandList) {
	// 視錐台のパラメータ計算
	const float nearHalfHeight = std::tan(fov_ * 0.5f) * zNear_;
	const float nearHalfWidth  = nearHalfHeight * aspectRatio_;
	const float farHalfHeight  = std::tan(fov_ * 0.5f) * zFar_;
	const float farHalfWidth   = farHalfHeight * aspectRatio_;

	// カメラ空間での近面と遠面の各頂点 (+Z方向)
	const Vec4 ntl(-nearHalfWidth, nearHalfHeight, zNear_, 1.0f);
	const Vec4 ntr(nearHalfWidth, nearHalfHeight, zNear_, 1.0f);
	const Vec4 nbl(-nearHalfWidth, -nearHalfHeight, zNear_, 1.0f);
	const Vec4 nbr(nearHalfWidth, -nearHalfHeight, zNear_, 1.0f);

	const Vec4 ftl(-farHalfWidth, farHalfHeight, zFar_, 1.0f);
	const Vec4 ftr(farHalfWidth, farHalfHeight, zFar_, 1.0f);
	const Vec4 fbl(-farHalfWidth, -farHalfHeight, zFar_, 1.0f);
	const Vec4 fbr(farHalfWidth, -farHalfHeight, zFar_, 1.0f);

	// カメラのワールド位置を取得
	const Vec3 cameraWorldPos = transform_->GetWorldPos();

	// ローカル座標からワールド座標へ変換
	const Vec4 wntl = worldMat_.Inverse() * ntl + Vec4(cameraWorldPos, 0.0f);
	const Vec4 wntr = worldMat_.Inverse() * ntr + Vec4(cameraWorldPos, 0.0f);
	const Vec4 wnbl = worldMat_.Inverse() * nbl + Vec4(cameraWorldPos, 0.0f);
	const Vec4 wnbr = worldMat_.Inverse() * nbr + Vec4(cameraWorldPos, 0.0f);
	const Vec4 wftl = worldMat_.Inverse() * ftl + Vec4(cameraWorldPos, 0.0f);
	const Vec4 wftr = worldMat_.Inverse() * ftr + Vec4(cameraWorldPos, 0.0f);
	const Vec4 wfbl = worldMat_.Inverse() * fbl + Vec4(cameraWorldPos, 0.0f);
	const Vec4 wfbr = worldMat_.Inverse() * fbr + Vec4(cameraWorldPos, 0.0f);

	// 近面のエッジ描画
	Debug::DrawLine({wntl.x, wntl.y, wntl.z}, {wntr.x, wntr.y, wntr.z},
	                Vec4::white);
	Debug::DrawLine({wntr.x, wntr.y, wntr.z}, {wnbr.x, wnbr.y, wnbr.z},
	                Vec4::white);
	Debug::DrawLine({wnbr.x, wnbr.y, wnbr.z}, {wnbl.x, wnbl.y, wnbl.z},
	                Vec4::white);
	Debug::DrawLine({wnbl.x, wnbl.y, wnbl.z}, {wntl.x, wntl.y, wntl.z},
	                Vec4::white);

	// 遠面のエッジ描画
	Debug::DrawLine({wftl.x, wftl.y, wftl.z}, {wftr.x, wftr.y, wftr.z},
	                Vec4::white);
	Debug::DrawLine({wftr.x, wftr.y, wftr.z}, {wfbr.x, wfbr.y, wfbr.z},
	                Vec4::white);
	Debug::DrawLine({wfbr.x, wfbr.y, wfbr.z}, {wfbl.x, wfbl.y, wfbl.z},
	                Vec4::white);
	Debug::DrawLine({wfbl.x, wfbl.y, wfbl.z}, {wftl.x, wftl.y, wftl.z},
	                Vec4::white);

	// 近面と遠面を接続するエッジ描画
	Debug::DrawLine({wntl.x, wntl.y, wntl.z}, {wftl.x, wftl.y, wftl.z},
	                Vec4::white);
	Debug::DrawLine({wntr.x, wntr.y, wntr.z}, {wftr.x, wftr.y, wftr.z},
	                Vec4::white);
	Debug::DrawLine({wnbl.x, wnbl.y, wnbl.z}, {wfbl.x, wfbl.y, wfbl.z},
	                Vec4::white);
	Debug::DrawLine({wnbr.x, wnbr.y, wnbr.z}, {wfbr.x, wfbr.y, wfbr.z},
	                Vec4::white);
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
		if (ImGui::DragFloat("FOV##cam", &fovTmp, 0.1f, kFovMin * Math::rad2Deg,
		                     kFovMax * Math::rad2Deg, "%.2f [deg]")) {
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

void CameraComponent::SetViewMat(const Mat4& mat4) {
	viewMat_ = mat4;
}
