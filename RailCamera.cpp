#include "RailCamera.h"

#include <cmath>

#include "imgui/imgui.h"

void RailCamera::Initialize(Camera* camera, const Vec3 position, const Vec3 rotation) {
	camera_ = camera;

	camera_->SetPos(position);
	camera_->SetRotate(rotation);
}

void RailCamera::Update() {
	//worldTransform_.translation_ += {0.0f, 0.0f, 0.01f};

	camera_->SetPos(Math::CatmullRomPosition(controlPoints_, time_));

	// 次のフレームのカメラ位置
	Vec3 nextPosition = Math::CatmullRomPosition(controlPoints_, time_ + 0.0002f);

	// カメラの前方向ベクトル
	Vec3 forward = (nextPosition - camera_->GetPos()).Normalized();

	// 前方向ベクトルから回転を計算
	// Z軸方向のベクトルが前を向くようにする
	float pitch = std::asin(-forward.y); // 上下方向
	float yaw = std::atan2(forward.x, forward.z); // 左右方向

	// 回転を設定
	//worldTransform_.rotation_.x = pitch; // X軸回転 (上下)
	//worldTransform_.rotation_.y = yaw;   // Y軸回転 (左右)

	camera_->SetRotate({ pitch,yaw, 0.0f });

	time_ += 0.00005f;

	// カメラオブジェクトのワールド行列からビュー行列を計算する
	//viewProjection_.matView = Matrix4x4::Inverse(worldTransform_.matWorld_);

	ImGui::Begin("Camera");

	//ImGui::DragFloat3("Pos", &.translation_.x, 0.01f);
	//ImGui::DragFloat3("Rot", &worldTransform_.rotation_.x, 0.01f);

	ImGui::End();

	//	worldTransform_.UpdateMatrix();
}
