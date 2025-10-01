#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Camera/CameraManager.h>
#include <engine/Debug/Debug.h>
#include <engine/Entity/Entity.h>
#include <engine/ImGui/ImGuiManager.h>

CameraComponent::~CameraComponent() = default;

//-----------------------------------------------------------------------------
// Purpose: コンポーネントがエンティティにアタッチされたときに呼び出されます
//-----------------------------------------------------------------------------
void CameraComponent::OnAttach(Entity& owner) {
	Component::OnAttach(owner);
	// 親からTransformComponentを取得
	mScene = mOwner->GetTransform();

	mAspectRatio = 16.0f / 9.0f;
	mWorldMat    = mScene->GetWorldMat();
	mViewMat     = mWorldMat.Inverse();
	mProjMat     = Mat4::PerspectiveFovMat(mFov, mAspectRatio, mZNear, mZFar);
	mViewProjMat = mViewMat * mProjMat;
}

//-----------------------------------------------------------------------------
// Purpose: コンポーネントの更新処理を行います
//-----------------------------------------------------------------------------
void CameraComponent::Update([[maybe_unused]] float deltaTime) {
	// 変更があった場合のみ更新
	//	if (transform_->IsDirty()) {
	const Vec3&       pos   = mScene->GetWorldPos();
	const Vec3&       scale = mScene->GetWorldScale();
	const Quaternion& rot   = mScene->GetWorldRot();

	const Mat4 S = Mat4::Scale(scale);
	const Mat4 R = Mat4::FromQuaternion(rot);
	const Mat4 T = Mat4::Translate(pos);

	mWorldMat    = R * S * T;
	mViewMat     = mWorldMat.Inverse();
	mProjMat     = Mat4::PerspectiveFovMat(mFov, mAspectRatio, mZNear, mZFar);
	mViewProjMat = mViewMat * mProjMat;

	mScene->SetIsDirty(false);
}

//-----------------------------------------------------------------------------
// Purpose: 描画処理のあるコンポーネントはこの関数をオーバーライドします
//-----------------------------------------------------------------------------
void CameraComponent::Render(
	[[maybe_unused]] ID3D12GraphicsCommandList* commandList) {
}

//-----------------------------------------------------------------------------
// Purpose: エディターのインスペクターに描画します
//-----------------------------------------------------------------------------
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

		float fovTmp = mFov * Math::rad2Deg;
		if (ImGui::DragFloat("FOV##cam", &fovTmp, 0.1f, kFovMin * Math::rad2Deg,
		                     kFovMax * Math::rad2Deg, "%.2f [deg]")) {
			SetFovVertical(fovTmp * Math::deg2Rad);
		}

		ImGui::DragFloat("ZNear##cam", &mZNear, 0.1f);
		ImGui::DragFloat("ZFar##cam", &mZFar, 0.1f);
		ImGui::DragFloat("AspectRatio##cam", &mAspectRatio, 0.1f);
	}
#endif
}

//-----------------------------------------------------------------------------
// Purpose: 垂直視野角を取得します [rad]
//-----------------------------------------------------------------------------
float& CameraComponent::GetFovVertical() {
	return mFov;
}

//-----------------------------------------------------------------------------
// Purpose: 垂直視野角を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetFovVertical(const float& newFovVertical) {
	mFov = std::clamp(newFovVertical, kFovMin, kFovMax);
}

//-----------------------------------------------------------------------------
// Purpose: ニアクリップ距離を取得します
//-----------------------------------------------------------------------------
float& CameraComponent::GetZNear() {
	return mZNear;
}

//-----------------------------------------------------------------------------
// Purpose: ニアクリップ距離を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetNearZ(const float& newNearZ) {
	mZNear = newNearZ;
}

//-----------------------------------------------------------------------------
// Purpose: ファークリップ距離を取得します
//-----------------------------------------------------------------------------
float& CameraComponent::GetZFar() {
	return mZFar;
}

//-----------------------------------------------------------------------------
// Purpose: ファークリップ距離を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetFarZ(const float& newFarZ) {
	mZFar = newFarZ;
}

//-----------------------------------------------------------------------------
// Purpose: ビュープロジェクション行列を取得します
//-----------------------------------------------------------------------------
Mat4& CameraComponent::GetViewProjMat() {
	return mViewProjMat;
}

//-----------------------------------------------------------------------------
// Purpose: ビュー行列を取得します
//-----------------------------------------------------------------------------
Mat4& CameraComponent::GetViewMat() {
	return mViewMat;
}

//-----------------------------------------------------------------------------
// Purpose: プロジェクション行列を取得します
//-----------------------------------------------------------------------------
Mat4& CameraComponent::GetProjMat() {
	return mProjMat;
}

//-----------------------------------------------------------------------------
// Purpose: アスペクト比を取得します
//-----------------------------------------------------------------------------
float& CameraComponent::GetAspectRatio() {
	return mAspectRatio;
}

//-----------------------------------------------------------------------------
// Purpose: アスペクト比を設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetAspectRatio(const float newAspectRatio) {
	mAspectRatio = newAspectRatio;
}

//-----------------------------------------------------------------------------
// Purpose: ビューマトリックスを設定します
//-----------------------------------------------------------------------------
void CameraComponent::SetViewMat(const Mat4& mat4) {
	mViewMat = mat4;
}
