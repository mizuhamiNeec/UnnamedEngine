#pragma once

#include <Components/CharacterMovement.h>
#include <Components/Base/Component.h>

#include <Lib/Math/Vector/Vec3.h>

class TransformComponent;

class PlayerMovement : public CharacterMovement {
public:
	~PlayerMovement() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	void Move() override;
	void ProcessInput();
	bool CheckGrounded();

	void SetVelocity(Vec3 newVel);

private:
	// プレイヤーの移動入力
	Vec3 moveInput_ = Vec3::zero;
};