#include "TransformComponent.h"

#include <ostream>

#include "../../Entity/Base/BaseEntity.h"

TransformComponent::TransformComponent(BaseEntity* owner) :
	::BaseComponent(owner),
	localPos_(Vec3::zero),
	localRot_(Quaternion::identity),
	localScale_(Vec3::one),
	pos_(Vec3::zero),
	rot_(Quaternion::identity),
	scale_(Vec3::one),
	parent_(nullptr),
	isDirty_(true) {
}

void TransformComponent::Initialize() {
}

void TransformComponent::Update([[maybe_unused]] float deltaTime) {
}

void TransformComponent::Terminate() {
}

void TransformComponent::Serialize([[maybe_unused]] std::ostream& out) const {
}

void TransformComponent::Deserialize([[maybe_unused]] std::istream& in) {
}

void TransformComponent::ImGuiDraw() {
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void TransformComponent::Translate(const Vec3& translation) {
	localPos_ += translation;
	MarkDirty();
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
void TransformComponent::Rotate(const Quaternion& rotation) {
	localRot_ = rotation * localRot_;
	MarkDirty();
}

void TransformComponent::SetParent(TransformComponent* newParent) {
	if (parent_ != newParent) {
		if (parent_) {
			parent_->RemoveChild(this);
		}
		parent_ = newParent;
		if (parent_) {
			parent_->AddChild(this);
		}
		MarkDirty();
	}
}

void TransformComponent::AddChild(TransformComponent* child) {
	if (std::ranges::find(children_, child) == children_.end()) {
		children_.push_back(child);
	}
}

void TransformComponent::RemoveChild(TransformComponent* child) {
	auto it = std::ranges::find(children_, child);
	if (it != children_.end()) {
		children_.erase(it);
	}
}

void TransformComponent::DetachFromParent() {
	if (parent_) {
		pos_ = parent_->TransformPoint(localPos_);
		rot_ = parent_->rot_ * localRot_;
		scale_ = parent_->scale_ * localScale_;

		parent_->RemoveChild(this);
		parent_ = nullptr;
	}
}

Vec3 TransformComponent::TransformPoint(const Vec3& point) const {
	return Mat4::Transform(point, localToWorldMat_);
}

Vec3 TransformComponent::TransformDirection(const Vec3& direction) const {
	return Mat4::Transform(direction, localToWorldMat_) - Mat4::Transform(Vec3::zero, localToWorldMat_);
}

Vec3 TransformComponent::InverseTransformPoint(const Vec3& point) const {
	return Mat4::Transform(point, worldToLocalMat_);
}

Vec3 TransformComponent::InverseTransformDirection(const Vec3& direction) const {
	return Mat4::Transform(direction, worldToLocalMat_) - Mat4::Transform(Vec3::zero, worldToLocalMat_);
}

void TransformComponent::UpdateMatrix() {
	// 更新する必要がない
	if (!isDirty_) {
		return;
	}

	// ローカル→ワールド変換行列
	Mat4 localMat = Mat4::Scale(localScale_) * Mat4::RotateQuaternion(localRot_) * Mat4::Translate(localPos_);

	// 親がいる場合
	if (parent_) {
		localToWorldMat_ = parent_->localToWorldMat_ * localMat;
		pos_ = parent_->pos_ + (parent_->rot_ * (parent_->scale_ * localPos_));
		rot_ = parent_->rot_ * localRot_;
		scale_ = parent_->scale_ * localScale_;
	} else {
		localToWorldMat_ = localMat;
		pos_ = localPos_;
		rot_ = localRot_;
		scale_ = localScale_;
	}

	// 逆行列の計算
	worldToLocalMat_ = localToWorldMat_.Inverse();

	// 子の更新
	for (auto* child : children_) {
		child->UpdateMatrix();
	}

	isDirty_ = false;
}

void TransformComponent::MarkDirty() {
	isDirty_ = true;
	for (auto* child : children_) {
		child->MarkDirty(); // 再帰的に子ノードのフラグも更新
	}
}

TransformComponent* TransformComponent::GetParent() const {
	return parent_;
}

const std::vector<TransformComponent*>& TransformComponent::GetChildren() const {
	return children_;
}

const Mat4& TransformComponent::GetLocalToWorldMatrix() const {
	return localToWorldMat_;
}

const Mat4& TransformComponent::GetWorldToLocalMatrix() const {
	return worldToLocalMat_;
}

const Vec3& TransformComponent::GetLocalPosition() const {
	return localPos_;
}

const Quaternion& TransformComponent::GetLocalRotation() const {
	return localRot_;
}

const Vec3& TransformComponent::GetLocalScale() const {
	return localScale_;
}

const Vec3& TransformComponent::GetWorldPosition() const {
	return pos_;
}

const Quaternion& TransformComponent::GetWorldRotation() const {
	return rot_;
}

const Vec3& TransformComponent::GetWorldScale() const {
	return scale_;
}

void TransformComponent::SetWorldPos(const Vec3& newWorldPos) {
	if (parent_) {
		// 親がある場合、ワールド座標からローカル座標に変換
		localPos_ = parent_->InverseTransformPoint(newWorldPos);
	} else {
		// 親がない場合、ワールド座標がそのままローカル座標
		localPos_ = newWorldPos;
	}
	MarkDirty();
}

void TransformComponent::SetWorldRot(const Quaternion& newWorldRot) {
	if (parent_) {
		// 親がある場合、ワールド回転から相対回転を計算
		localRot_ = parent_->rot_.Inverse() * newWorldRot;
	} else {
		// 親がない場合、ワールド回転がそのままローカル回転
		localRot_ = newWorldRot;
	}
	MarkDirty();
}

void TransformComponent::SetWorldScale(const Vec3& newWorldScale) {
	if (parent_) {
		// 親がある場合、ワールドスケールから相対スケールを計算
		localScale_ = newWorldScale / parent_->scale_;
	} else {
		// 親がない場合、ワールドスケールがそのままローカルスケール
		localScale_ = newWorldScale;
	}
	MarkDirty();
}

void TransformComponent::SetLocalPos(const Vec3& newLocalPos) {
	localPos_ = newLocalPos;
	MarkDirty();
}

void TransformComponent::SetLocalRot(const Quaternion& newLocalRot) {
	localRot_ = newLocalRot;
	MarkDirty();
}

void TransformComponent::SetLocalScale(const Vec3& newLocalScale) {
	localScale_ = newLocalScale;
	MarkDirty();
}