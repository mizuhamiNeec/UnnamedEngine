#pragma once
#include <Components/Base/Component.h>

#include "Lib/Math/Vector/Vec3.h"

class TransformComponent;

class CharacterMovement : public Component {
public:
	~CharacterMovement() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	virtual void Move();
	void ApplyHalfGravity();
	void ApplyFriction();
	bool CheckGrounded() const;

	void Accelerate(Vec3 dir, float speed, float accel);
	void AirAccelerate(Vec3 dir, float speed, float accel);

	void CheckVelocity();

	[[nodiscard]] Vec3 GetVelocity() const;

protected:
	TransformComponent* transform_ = nullptr;

	float deltaTime_ = 0.0f;

	Vec3 position_ = Vec3::zero;
	Vec3 velocity_ = Vec3::zero;

	float speed_ = 0.0f;
	float jumpVel_ = 0.0f;

	bool isGrounded_ = false;
};
