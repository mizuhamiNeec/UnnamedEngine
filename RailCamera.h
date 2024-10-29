#pragma once
#include "../Camera/Camera.h"

struct Vec3;

class RailCamera {
public:
	void Initialize(Camera* camera, Vec3 position, Vec3 rotation);

	void Update();

private:
	float time_ = 0.0f;

	Camera* camera_ = nullptr;
};
