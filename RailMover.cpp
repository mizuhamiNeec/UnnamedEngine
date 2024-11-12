#include "RailMover.h"
#include <cmath>

#ifdef _DEBUG
#include "imgui/imgui.h"
#endif

#include "Source/Engine/Object3D/Object3D.h"

void RailMover::Initialize(Object3D* object, const std::vector<Vec3>& points, Vec3 position, Vec3 rotation) {
	object_ = object;

	object_->SetPos(position);
	object_->SetRot(rotation);

	points_ = points;
	// ループのために末尾に最初の点を追加する
	points_.push_back(points_[0]);
}

void RailMover::Update([[maybe_unused]] const float& deltaTime) {

#ifdef _DEBUG
	ImGui::Begin("Rail");
	if (ImGui::DragFloat("t", &t_, 0.01f)) {
		if (t_ < 0.0f) {
			t_ = 0.0f;
		}
	}
	ImGui::End();
#endif

	// t_の進行をdeltaTimeに基づいて更新
	t_ += speed_ * deltaTime;

	// 制御点間の区間を超えた場合
	int segmentCount = static_cast<int>(points_.size() - 3); // points_.size()が4以上であることを前提
	int segment = static_cast<int>(t_);
	if (segment >= segmentCount) {
		t_ = 0.0f;  // ループさせるためにt_をリセット
		segment = 0;
	}

	float localT = t_ - segment;

	// Catmull-Romスプラインを使って位置を計算
	Vec3 p0 = points_[segment];
	Vec3 p1 = points_[segment + 1];
	Vec3 p2 = points_[segment + 2];
	Vec3 p3 = points_[segment + 3];
	Vec3 newPosition = Math::CatmullRom(p0, p1, p2, p3, localT);

	// オブジェクトの回転を更新
	Vec3 direction = newPosition - object_->GetPos();
	direction.Normalize();
	float pitch = -atan2(direction.y, sqrt(direction.x * direction.x + direction.z * direction.z));
	float yaw = atan2(direction.x, direction.z);
	Vec3 newRotation = Vec3(pitch, yaw, 0.0f);

	object_->SetPos(newPosition);
	object_->SetRot(newRotation);
}
