#pragma once
#include "../Camera/Camera.h"

struct Vec3;

class RailCamera {
public:
	void Initialize(Camera* camera, Vec3 position, Vec3 rotation);

	void Update();

private:
	// 距離からtを求める補間関数
	float DistanceToT(float distance) const;

	// 現在の傾斜角度を計算（ラジアン）
	static float CalculateSlope(float currentT);

	// 傾斜に基づいて加速度を計算
	float CalculateAcceleration(float slope) const;

	void SetBaseSpeed(float speedMetersPerSecond);

	// 現在の速度を取得（デバッグ用）
	float GetCurrentSpeed() const;

	float splineLength_ = 0.0f;
	float distanceTraveled_ = 0.0f;
	float baseSpeed_ = 2.0f;  // 平地での基本速度（メートル/秒）
	float currentSpeed_ = 0.0f;  // 現在の速度
	const float gravity_ = 9.81f;  // 重力加速度（メートル/秒^2）
	const float friction_ = 0.02f;  // 摩擦係数
	std::vector<Vec3> controlPoints_;
	std::vector<std::pair<float, float>> distanceToTMap_;

	float time_ = 0.0f;

	Camera* camera_ = nullptr;
};
