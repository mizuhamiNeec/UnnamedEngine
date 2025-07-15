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
	void         ApplyFriction(float fricValue);
	bool         CheckGrounded();

	void Accelerate(Vec3 dir, float speed, float accel);
	void AirAccelerate(Vec3 dir, float speed, float accel);

	bool IsGrounded() const;

	void CheckVelocity();

	[[nodiscard]] Vec3 GetVelocity() const;

	[[nodiscard]] virtual Vec3 GetHeadPos() const;

protected:
	TransformComponent* mTransform = nullptr;

	Vec3 mPosition = Vec3::zero;
	Vec3 mVelocity = Vec3::zero;

	float mSpeed   = 0.0f;
	float mJumpVel = 0.0f;
	
	bool bIsGrounded   = false;
	bool bWasGrounded  = false;
	Vec3 mGroundNormal = Vec3::zero;
	
	const float kDefaultHeightHU = 73.0f;
	const float kDefaultWidthHU  = 33.0f;
	float mCurrentHeightHU = 73.0f;
	float mCurrentWidthHU  = 33.0f;
	
	bool bWishCrouch  = false;
	bool bIsCrouching = false;

	enum SlideState {
		NotSliding,
		Sliding,
		SlideRecovery
	};

	SlideState mSlideState         = NotSliding;
	float      mSlideTimer         = 0.0f;
	float      mSlideRecoveryTimer = 0.0f;

	const float mSlideMinSpeed     = 180.0f; // スライドを維持するための最小速度
	const float mSlideBoost        = 1.5f;   // スライド中の加速量
	const float mSlideFriction     = 0.25f;  // スライド中の摩擦係数
	const float mMaxSlideTime      = 1.0f;   // スライドの最大維持時間(秒)
	const float mSlideRecoveryTime = 0.5f;   // スライド後の回復時間(秒)
	const float mSlideDownForce    = 150.0f; // スライド中の下方向への力

	Vec3 mSlideDir = Vec3::zero; // スライド中の方向ベクトル
};
