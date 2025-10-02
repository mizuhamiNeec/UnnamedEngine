#pragma once
#include <engine/Components/base/Component.h>
#include <engine/uprimitive/UPrimitives.h>
#include <runtime/core/math/Vec3.h>

namespace UPhysics {
	class Engine;
}

class GameMovementComponent : public Component {
public:
	GameMovementComponent() = default;
	~GameMovementComponent() override;

	void SetUPhysicsEngine(UPhysics::Engine* engine);

	Vec3 GetHeadPos() const;

	[[nodiscard]] Vec3 GetVelocity() const;
	void               SetVelocity(Vec3 newVel);

	// Component
	void OnAttach(Entity& owner) override;
	void OnDetach() override;

	void PrePhysics(float deltaTime) override;
	void Update(float deltaTime) override;
	void PostPhysics(float deltaTime) override;

	void Render(ID3D12GraphicsCommandList* commandList) override;

	void DrawInspectorImGui() override;

	[[nodiscard]] bool    IsEditorOnly() const override;
	[[nodiscard]] Entity* GetOwner() const override;

	[[nodiscard]] bool IsGrounded() const { return mIsGrounded; }

private:
	void ProcessInput();
	void CalcMoveDir();


	struct GroundContact {
		bool  onGround = false;
		Vec3  normal = Vec3::zero;
		float distance = 0.0f;
	};

	static Vec3 BlendNormal(
		const Vec3& now, const Vec3& prev,
		float       blendHz, float   deltaTime
	);
	Vec3          ConsumeSnapOffset();
	GroundContact CheckGrounded();

	struct MoveResult {
		Vec3 endPos{};
		Vec3 endVel{};
		bool hitSomething = false;
		bool hitGround = false;
		Vec3 groundNormal = Vec3::up;
		int  numBumps = 0;
	};

	Vec3 ClipVelocityToPlane(const Vec3& v, const Vec3& n, float overBounce);
	bool IsWalkable(const Vec3& n, const Vec3& up, float maxSlopeDeg);
	float MinGroundDot(float maxSlopeDeg);
	bool IsCeiling(const Vec3& n, const Vec3& up, float maxSlopeDeg);
	static Vec3 ProjectOntoPlane(const Vec3& v, const Vec3& n);
	MoveResult SlideMove(
		Vec3                 startPos,
		Vec3                 startVel,
		const Unnamed::Box& shape,
		float                dt,
		const Vec3& up
	);

	void Jump();
	void ApplyHalfGravity();
	void Friction();
	void Accelerate(const Vec3& dir, float speed, float accel);
	void AirAccelerate(const Vec3& dir, float speed, float accel);
	void CheckVelocity();

protected:
	UPhysics::Engine* mUPhysicsEngine = nullptr;

	// Movement
	Vec3 mVecWishVel;
	Vec3 mVecPosition = Vec3::zero;
	Vec3 mVecVelocity = Vec3::zero;

	// param
	float mSpeed = 320.0f;
	float mJumpVel = 300.0f;

	// Input
	Vec3 mVecMoveInput = Vec3::zero;
	bool mWishJump;
	bool mWishCrouch;

	float mDeltaTime;

	const float kDefaultHeightHU = 73.0f;
	const float kDefaultWidthHU = 33.0f;
	float       mCurrentHeightHu = kDefaultHeightHU;
	float       mCurrentWidthHu = kDefaultWidthHU;

	// Ground
	float mTimeSinceLost = 999.0f;   // 最後に地面を離れてからの時間
	Vec3  mPrevGroundNormal = Vec3::up; // 最後の接地法線
	Vec3  mSnapOffset = Vec3::zero;
	bool  mIsGrounded = false; // 接地フラグ

	bool  mPrevJumpDown = false; // 前フレームの押下状態
	bool  mPressedJumpThisFrame = false; // 立ち上がりエッジ
	float mUngroundTimer = 0.0f;  // 再接地禁止の残り時間[s]
	bool  mWasGrounded = false;
	bool  mJustAutoJumped = false;
	float mJumpBufferTimer = 0.0f;
	float mSinceLanded = 0.0f;

	bool                   mAutoBHopEnabled = true;
	static constexpr float kJumpBufferTimeSec = 0.15f;  // 150ms
	static constexpr float kBHopFrictionSkipSec = 0.015f; // 15ms(大体1フレ
	static constexpr float kJumpUngroundTime = 0.10f;  // 100ms
	const float            kBHopSpeedCap = 0.0f;   // 無制限

	// Resolver
	int   kMaxPlanes = 5;
	int   kMaxBumps = 4;
	float kPushOutHU = 0.25f;
	float kOverBounce = 1.0f;
	float kMaxSlopeDeg = 45.0f;
	float kStepHeightHU = 18.0f;
	float kSnapUpEpsHU = 0.5f;
	bool  kRampBoostEnabled = false;
	float kUnstickEpsHU = 0.5f;
	float kPushOutEpsHU = 0.25f;
};
