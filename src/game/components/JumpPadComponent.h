#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>

#include "engine/ResourceSystem/Audio/Audio.h"

class AABBCollider;
class MovementComponent;

class JumpPadComponent final : public Component {
public:
	explicit JumpPadComponent(float boostVelocityHu = 800.0f);

	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	void SetBoostVelocityHu(float boostVelocityHu);
	[[nodiscard]] float GetBoostVelocityHu() const noexcept;

private:
	void CheckPlayerCollision();
	void ApplyBoost(MovementComponent& movement) const;

	AABBCollider* mCollider = nullptr;
	
	std::shared_ptr<Audio> mJumpPadSound;
	
	float         mBoostVelocityHu = 800.0f;
	bool          mWasInside = false;
};

