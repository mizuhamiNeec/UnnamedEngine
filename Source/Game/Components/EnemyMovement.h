#pragma once
#include "CharacterMovement.h"

class EnemyMovement final : public CharacterMovement {
public:
	~EnemyMovement() override;
	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	void Move() override;

	void SetMoveInput(Vec3 newMoveInput);

private:
	Vec3 moveInput_ = Vec3::zero;
};

