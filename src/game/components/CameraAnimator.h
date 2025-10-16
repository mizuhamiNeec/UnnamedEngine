#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>

class MovementComponent;
class CameraRotator;

// カメラアニメーション用のコンポーネント
// ジャンプ、スライディング、ウォールランなどの動きに応じてカメラを演出
class CameraAnimator : public Component {
public:
	void OnAttach(Entity& owner) override;
	void Init(MovementComponent* movementComponent, CameraRotator* cameraRotator);
	void Update(float dt) override;
	void DrawInspectorImGui() override;

private:
	void UpdateJumpAnimation(float dt);
	void UpdateDoubleJumpAnimation(float dt);
	void UpdateSlideAnimation(float dt);
	void UpdateWallrunAnimation(float dt);
	void UpdateLandingAnimation(float dt);
	void ApplyShakeAndTilt(float dt);

	MovementComponent* mMovement = nullptr;

	// Jump animation
	bool  mWasInAir           = false;
	float mJumpAnimTime       = 0.0f;
	float mDoubleJumpAnimTime = 0.0f;
	bool  mHadDoubleJump      = true;

	// Slide animation
	bool  mWasSliding      = false;
	float mSlideAnimTime   = 0.0f;
	float mSlideEntrySpeed = 0.0f;

	// Wallrun animation
	bool  mWasWallRunning  = false;
	float mWallrunAnimTime = 0.0f;
	float mWallrunSide     = 0.0f; // -1 = left, +1 = right

	// Landing animation
	bool  mLandingActive    = false;
	float mLandingAnimTime  = 0.0f;
	float mLandingIntensity = 0.0f;

	// Current shake/tilt values
	Vec3  mCurrentShake = Vec3::zero;
	float mCurrentRoll  = 0.0f; // Z軸回転（傾き）
	float mCurrentPitch = 0.0f; // X軸回転（上下）
	
	// Reference to CameraRotator for applying rotation offsets
	CameraRotator* mCameraRotator = nullptr;
	
	// Perlin noise time accumulator
	float mNoiseTime = 0.0f;
	
	// Helper function for Perlin noise
	float PerlinNoise(float x, float y, float z) const;

	// Animation parameters (Titanfall 2 style)
	static constexpr float kJumpShakeAmount   = 0.07f;
	static constexpr float kJumpShakeDuration = 0.2f;

	static constexpr float kDoubleJumpShakeAmount   = 0.1f;
	static constexpr float kDoubleJumpShakeDuration = 0.3f;

	static constexpr float kSlideRollAmount  = 2.0f; // degrees
	static constexpr float kSlideShakeAmount = 0.01f;
	static constexpr float kSlideRollSpeed   = 15.0f; // より速く傾く

	static constexpr float kWallrunRollAmount  = 5.0f; // degrees
	static constexpr float kWallrunRollSpeed   = 20.0f; // より速く傾く
	static constexpr float kWallrunShakeAmount = 0.008f;

	static constexpr float kLandingShakeAmount   = 0.4f;
	static constexpr float kLandingShakeDuration = 0.25f;
	static constexpr float kLandingMinSpeed      = 3.0f; // m/s
	static constexpr float kLandingPitchAmount   = 4.0f; // degrees - 着地時に下を向く
	
	// Jump pitch animation
	static constexpr float kJumpPitchAmount       = 3.0f; // degrees - ジャンプ時に上を向く
	static constexpr float kDoubleJumpPitchAmount = 5.0f; // degrees - ダブルジャンプ時にさらに上を向く

	static constexpr float kShakeFrequency = 15.0f;
};
