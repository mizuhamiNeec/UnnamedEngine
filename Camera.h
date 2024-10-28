#pragma once

struct Transform;
struct Mat4;

class Camera {
public:
	void Update();
private:
	Transform transform_;
	Mat4 worldMat_;
	Mat4 viewMat_;

	Mat4 projMat_;
	float hFov_;
	float aspectRatio_;
	float nearClipDist_;
	float farClipDist_;
};
