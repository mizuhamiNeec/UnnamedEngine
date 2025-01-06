#pragma once

#include <xstring>
#include <Components/Base/Component.h>

#include <Components/CharacterMovement.h>
#include "Lib/Math/Vector/Vec3.h"

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

private:
	// プレイヤーの移動入力
	Vec3 moveInput_ = Vec3::zero;
};
