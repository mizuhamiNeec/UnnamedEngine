#pragma once

#include <xstring>
#include <Components/Base/Component.h>

#include "Lib/Math/Vector/Vec3.h"

class TransformComponent;

class PlayerMovement : public Component {
public:
	~PlayerMovement() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;


	void ProcessInput();
	void FullWalkMove();
	void ApplyHalfGravity();
	void ApplyFriction();
	bool CheckGrounded() const;

	void Accelerate(Vec3 wishdir, float wishspeed, float accel);
	void AirAccelerate(Vec3 wishdir, float wishspeed, float accel);

	void CheckVelocity();

	Vec3 GetVelocity() const;

private:
	TransformComponent* transform_ = nullptr;

	float deltaTime_ = 0.0f;

	// プレイヤーの位置
	Vec3 position_ = Vec3::zero;
	// プレイヤーの速度
	Vec3 velocity_ = Vec3::zero;

	// プレイヤーの移動入力
	Vec3 moveInput_ = Vec3::zero;

	float speed_ = 320.0f;
	float jumpVel_ = 300.0f;

	bool isGrounded_ = false;
};
