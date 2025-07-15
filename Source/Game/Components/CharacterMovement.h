#pragma once
#include <Components/Base/Component.h>

#include <Lib/Math/Vector/Vec3.h>

class TransformComponent;

class CharacterMovement : public Component {
public:
	~CharacterMovement() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	virtual void Move();
	void         ApplyHalfGravity();
	void         ApplyFriction(float frictionValue);
	bool         CheckGrounded();

	void Accelerate(Vec3 dir, float speed, float accel);
	void AirAccelerate(Vec3 dir, float speed, float accel);

	[[nodiscard]] bool IsGrounded() const;

	void CheckVelocity();

	[[nodiscard]] Vec3 GetVelocity() const;

	[[nodiscard]] virtual Vec3 GetHeadPos() const;

protected:
	float mDeltaTime = 0.0f;
	Vec3  mPosition  = Vec3::zero;
	Vec3  mVelocity  = Vec3::zero;

	float mSpeed   = 0.0f;
	float mJumpVel = 0.0f;

	bool mIsGrounded   = false;
	bool mWasGrounded  = false;
	Vec3 mGroundNormal = Vec3::zero;

	const float kDefaultHeightHU = 73.0f;
	const float kDefaultWidthHU  = 33.0f;
	float       mCurrentHeightHu = 73.0f;
	float       mCurrentWidthHu  = 33.0f;

	bool mWishCrouch  = false;
	bool mIsCrouching = false;
};
