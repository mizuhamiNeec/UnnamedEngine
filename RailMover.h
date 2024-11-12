#pragma once
#include "../Camera/Camera.h"

class Object3D;
struct Vec3;

class RailMover {
public:
	void Initialize(Object3D* object, const std::vector<Vec3>& points, Vec3 position, Vec3 rotation);

	void Update(const float& deltaTime);

private:
	Object3D* object_ = nullptr;
	std::vector<Vec3> points_;

	float t_ = 0.0f;  // スプライン上の位置を示すパラメータ
	float speed_ = 0.15f;  // オブジェクトの移動速度
};
