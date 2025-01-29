#pragma once

#include <Components/CharacterMovement.h>
#include <Components/Base/Component.h>

#include <Lib/Math/Vector/Vec3.h>

#include "Lib/Math/Quaternion/Quaternion.h"

class TransformComponent;

class PlayerMovement : public CharacterMovement {
public:
	~PlayerMovement() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	void Move() override;
	void ProcessInput();
	void SetIsGrounded(bool bIsGrounded);

	Vec3 CollideAndSlide(const Vec3& vel, const Vec3& pos, int depth) override;

	void SetVelocity(Vec3 newVel);

private:
	int maxBounces = 5;
	float skinWidth = 0.015f;

	// プレイヤーの移動入力
	Vec3 moveInput_ = Vec3::zero;
	Vec3 wishdir_ = Vec3::zero;
};
