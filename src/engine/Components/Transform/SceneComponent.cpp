#include "engine/Components/Transform/SceneComponent.h"

#include "engine/Entity/Entity.h"
#include "engine/ImGui/ImGuiManager.h"

/// @brief 更新処理
/// @param deltaTime 前フレームからの経過時間（秒）
void SceneComponent::Update([[maybe_unused]] float deltaTime) {
}

/// @brief ローカル位置、回転、スケールの取得・設定
const Vec3& SceneComponent::GetLocalPos() const {
	return mPosition;
}

/// @brief ローカル位置の設定
void SceneComponent::SetLocalPos(const Vec3& newPosition) {
	mPosition = newPosition;
	MarkDirty();
}

/// @brief ローカル回転の取得・設定
const Quaternion& SceneComponent::GetLocalRot() const {
	return mRotation;
}

/// @brief ローカル回転の設定
/// @param newRotation 新しいローカル回転 
void SceneComponent::SetLocalRot(const Quaternion& newRotation) {
	mRotation = newRotation.Normalized();
	MarkDirty();
}

/// @brief ローカルスケールの取得・設定
/// @return ローカルスケール
const Vec3& SceneComponent::GetLocalScale() const {
	return mScale;
}

/// @brief ローカルスケールの設定
/// @param newScale 新しいローカルスケール
void SceneComponent::SetLocalScale(const Vec3& newScale) {
	mScale = newScale;
	MarkDirty();
}

/// @brief ワールド位置、回転、スケールの取得・設定
Vec3 SceneComponent::GetWorldPos() const {
	if (mOwner && mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			// 親のワールド情報を取得
			const Vec3       parentPos = parentTransform->GetWorldPos();
			const Quaternion parentRot = parentTransform->GetWorldRot();

			// ローカル位置を親の回転で変換
			const Vec3 rotatedPos = parentRot * mPosition;

			// 親の位置を加算
			return parentPos + rotatedPos;
		}
	}
	return mPosition;
}

/// @brief ワールド回転の取得
/// @return ワールド回転
Quaternion SceneComponent::GetWorldRot() const {
	if (mOwner && mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldRot() * mRotation;
		}
	}
	return mRotation;
}

/// @brief ワールドスケールの取得
/// @return ワールドスケール
Vec3 SceneComponent::GetWorldScale() const {
	if (mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldScale() * mScale;
		}
	}
	return mScale;
}

/// @brief ワールド位置の設定
/// @param newPosition 新しいワールド位置
void SceneComponent::SetWorldPos(const Vec3& newPosition) {
	if (mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			// 親の逆行列を用いてワールド座標をローカル座標に変換
			const Mat4 parentWorldMat = parentTransform->GetWorldMat();
			const Mat4 parentInvMat = parentWorldMat.Inverse();
			mPosition = parentInvMat.TransformPoint(newPosition);
			MarkDirty();
			return;
		}
	}
	// 親がいない場合はそのまま設定
	mPosition = newPosition;
	MarkDirty();
}

/// @brief ワールド回転の設定
/// @param newRotation 新しいワールド回転
void SceneComponent::SetWorldRot(const Quaternion& newRotation) {
	if (mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			// 親の回転を取得
			Quaternion parentRot = parentTransform->GetWorldRot();
			// 親の回転の逆クォータニオンをかけることでローカル回転を取得
			const Quaternion invParentRot = parentRot.Inverse();
			mRotation                     = invParentRot * newRotation;
			MarkDirty();
			return;
		}
	}
	mRotation = newRotation; // 親がいない場合はそのまま設定
	MarkDirty();
}

/// @brief ワールドスケールの設定
/// @param newScale 新しいワールドスケール
void SceneComponent::SetWorldScale(const Vec3& newScale) {
	if (mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			const Vec3 parentScale = parentTransform->GetWorldScale();
			mScale                 = newScale / parentScale; // 親スケールとの比率を設定
			MarkDirty();
			return;
		}
	}
	mScale = newScale; // 親がいない場合そのまま設定
	MarkDirty();
}

/// @brief ローカル変換行列の取得
/// @return ローカル変換行列
const Mat4& SceneComponent::GetLocalMat() const {
	if (mIsDirty) {
		RecalculateMat();
	}
	return mLocalMat;
}

/// @brief ワールド変換行列の取得
/// @return ワールド変換行列
Mat4 SceneComponent::GetWorldMat() const {
	if (mOwner && mOwner->GetParent()) {
		if (const SceneComponent* parentTransform = mOwner->GetParent()->
			GetTransform()) {
			return parentTransform->GetWorldMat() * GetLocalMat();
		}
	}
	return GetLocalMat();
}

/// @brief ImGuiでインスペクターを描画します。
void SceneComponent::DrawInspectorImGui() {
#ifdef _DEBUG
	ImGuiManager::EditTransform(*this, 0.1f);
#endif
}

/// @brief 変換情報が変更されたことをマークします。
void SceneComponent::MarkDirty() const {
	mIsDirty = true;
	for (const auto& child : mOwner->GetChildren()) {
		if (const auto* childTransform = child->GetTransform()) {
			childTransform->MarkDirty();
		}
	}
}

/// @brief 所有者エンティティの取得
/// @return 所有者エンティティ
Entity* SceneComponent::GetOwner() const {
	return mOwner;
}

/// @brief 変換情報が変更されたかどうかを取得します。
/// @return 変換情報が変更されたかどうか
bool SceneComponent::IsDirty() const {
	return mIsDirty;
}

/// @brief 変換情報の変更フラグを設定します。
/// @param newIsDirty 新しい変更フラグの値
void SceneComponent::SetIsDirty(const bool newIsDirty) const {
	mIsDirty = newIsDirty;
}

/// @brief ローカル変換行列を再計算します。
void SceneComponent::RecalculateMat() const {
	const Mat4 S = Mat4::Scale(mScale);
	const Mat4 R = Mat4::FromQuaternion(mRotation);
	const Mat4 T = Mat4::Translate(mPosition);

	mLocalMat = R * S * T;
	mIsDirty  = false;
}
