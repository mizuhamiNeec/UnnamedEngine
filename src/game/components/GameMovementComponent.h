#pragma once
#include "engine/public/Components/base/Component.h"
#include "engine/public/uphysics/PhysicsTypes.h"

#include <runtime/core/math/Math.h>

namespace UPhysics {
	class Engine;
}

class GameMovementComponent : public Component {
public:
	GameMovementComponent() = default;
	~GameMovementComponent() override;

	void SetUPhysicsEngine(UPhysics::Engine* engine);

	[[nodiscard]] Vec3 GetHeadPos() const;

	[[nodiscard]] Vec3 Velocity() const;
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

	[[nodiscard]] bool IsGrounded() const { return mMovementState.bIsGrounded; }

private:
	void ProcessInput();
	void CalcMoveDir();

	void ApplyHalfGravity();
	void Friction();
	void Accelerate(const Vec3& dir, float speed, float accel);
	void AirAccelerate(const Vec3& dir, float speed, float accel);
	void CheckVelocity();

protected:
	UPhysics::Engine* mUPhysicsEngine = nullptr;

	// 関数内で参照する用
	float mDeltaTime;

	// Input
	struct MoveInput {
		Vec2 vecMoveDir  = Vec2::zero;
		bool bWishJump   = false;
		bool bWishCrouch = false;
	};

	MoveInput mMoveInput;

	static constexpr float kDefaultHeightHU = 73.0f;
	static constexpr float kDefaultWidthHU  = 33.0f;

	static constexpr float kWalkSpeed   = 150.0f;
	static constexpr float kCrouchSpeed = 100.0f;
	static constexpr float kSprintSpeed = 290.0f;

	struct MovementState {
		Vec3 vecWishDir  = Vec3::zero;
		Vec3 vecVelocity = Vec3::zero;

		float height       = kDefaultHeightHU;
		float width        = kDefaultWidthHU;
		float currentSpeed = kWalkSpeed;
		bool  bIsGrounded  = false;
	};

	MovementState mMovementState;
};
