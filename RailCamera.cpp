#include "RailCamera.h"

#include <algorithm>
#include <cmath>

#include "imgui/imgui.h"

void RailCamera::Initialize(Camera* camera, const Vec3 position, const Vec3 rotation) {
	camera_ = camera;

	camera_->SetPos(position);
	camera_->SetRotate(rotation);

	controlPoints_ = controlPoints;
	currentSpeed_ = baseSpeed_;  // 初期速度を設定

	// スプラインの全長を計算し、距離とtのマッピングテーブルを作成
	const int samples = 1000;
	const float deltaT = 1.0f / samples;
	float accumulatedLength = 0.0f;
	Vec3 prevPos = Math::CatmullRomPosition(controlPoints, 0.0f);

	distanceToTMap_.push_back({ 0.0f, 0.0f });

	for (int i = 1; i <= samples; i++) {
		float t = i * deltaT;
		Vec3 currentPos = Math::CatmullRomPosition(controlPoints, t);
		accumulatedLength += (currentPos - prevPos).Length();

		distanceToTMap_.push_back({ accumulatedLength, t });
		prevPos = currentPos;
	}

	splineLength_ = accumulatedLength;
	distanceTraveled_ = 0.0f;
}

void RailCamera::Update() {
	float deltaTime = 1.0f / 60.0f;  // フレーム時間

	// 現在の位置でのtパラメータを取得
	float currentT = DistanceToT(distanceTraveled_);

	// 現在の傾斜を計算
	float slope = CalculateSlope(currentT);

	// 加速度を計算
	float acceleration = CalculateAcceleration(slope);

	// 速度を更新（加速度に基づく）
	currentSpeed_ += acceleration * deltaTime;

	// 最小速度と最大速度の制限（必要に応じて調整）
	const float minSpeed = 0.5f;  // 最小速度
	const float maxSpeed = 10.0f;  // 最大速度
	currentSpeed_ = std::clamp(currentSpeed_, minSpeed, maxSpeed);

	// 位置を更新
	distanceTraveled_ += currentSpeed_ * deltaTime;

	// スプラインの終端に達したら最初に戻る
	if (distanceTraveled_ >= splineLength_) {
		distanceTraveled_ = 0.0f;
	}

	// カメラの位置と向きを更新
	float nextT = DistanceToT(distanceTraveled_ + 0.1f);
	Vec3 currentPosition = Math::CatmullRomPosition(controlPoints_, currentT);
	Vec3 nextPosition = Math::CatmullRomPosition(controlPoints_, nextT);

	camera_->SetPos(currentPosition);

	Vec3 forward = (nextPosition - currentPosition).Normalized();
	float pitch = std::asin(-forward.y);
	float yaw = std::atan2(forward.x, forward.z);
	camera_->SetRotate({ pitch, yaw, 0.0f });
}

float RailCamera::DistanceToT(float distance) const {
	if (distance <= 0.0f) return 0.0f;
	if (distance >= splineLength_) return 1.0f;

	size_t left = 0;
	size_t right = distanceToTMap_.size() - 1;

	while (right - left > 1) {
		size_t mid = (left + right) / 2;
		if (distanceToTMap_[mid].first < distance) {
			left = mid;
		} else {
			right = mid;
		}
	}

	float t1 = distanceToTMap_[left].second;
	float t2 = distanceToTMap_[right].second;
	float d1 = distanceToTMap_[left].first;
	float d2 = distanceToTMap_[right].first;

	return t1 + (t2 - t1) * ((distance - d1) / (d2 - d1));
}

float RailCamera::CalculateSlope(const float currentT) {
	const float deltaT = 0.001f;
	Vec3 currentPos = Math::CatmullRomPosition(controlPoints, currentT);
	Vec3 nextPos = Math::CatmullRomPosition(controlPoints, currentT + deltaT);

	Vec3 direction = (nextPos - currentPos).Normalized();
	// Y軸との角度を計算（上り坂が正、下り坂が負）
	return -std::asin(direction.y);
}

float RailCamera::CalculateAcceleration(const float slope) const {
	// 斜面に沿った重力加速度の計算
	float accelerationDueToGravity = gravity_ * std::sin(slope);

	// 摩擦による減速
	float frictionDeceleration = -friction_ * gravity_ * std::cos(slope);
	if (currentSpeed_ > 0) {
		frictionDeceleration = -std::abs(frictionDeceleration);
	} else if (currentSpeed_ < 0) {
		frictionDeceleration = std::abs(frictionDeceleration);
	}

	return accelerationDueToGravity + frictionDeceleration;
}

void RailCamera::SetBaseSpeed(float speedMetersPerSecond) {
	baseSpeed_ = speedMetersPerSecond;
	currentSpeed_ = baseSpeed_;
}

float RailCamera::GetCurrentSpeed() const {
	return currentSpeed_;
}
