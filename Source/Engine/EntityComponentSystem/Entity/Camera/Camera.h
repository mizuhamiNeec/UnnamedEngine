#pragma once
#include "../Base/BaseEntity.h"

class CameraComponent;
class TransformComponent;

class Camera : public BaseEntity {
public:
	Camera();
	~Camera() = default;

	// Setters
	void SetWorldPos(const Vec3& newWorldPos) const;
	void SetWorldRot(const Quaternion& newWorldRot) const;
	void LookAt(const Vec3& target, const Vec3& up = Vec3::up);
	void SetFov(const float& fovDegrees) const;
	void SetNearZ(const float& newNearZ) const;
	void SetFarZ(const float& newFarZ) const;
	void SetAspectRatio(const float& newAspectRatio) const;

	// Getters
	Vec3 GetWorldPos() const;
	Quaternion GetRot() const;
	float GetFov() const;
	float GetNearZ() const;
	float GetFarZ() const;
	float GetAspectRatio() const;
	Mat4 GetViewMat() const;
	Mat4 GetProjMat() const;
	Mat4 GetViewProjMat() const;

	// Component getters
	TransformComponent* GetTransform() const;
	CameraComponent* GetCamera() const;
private:
	TransformComponent* transform_;
	CameraComponent* camera_;
};

