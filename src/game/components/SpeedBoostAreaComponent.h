#pragma once
#include <engine/Components/base/Component.h>
#include <runtime/core/math/Math.h>

class AABBCollider;
class MovementComponent;

class SpeedBoostAreaComponent final : public Component {
public:
	SpeedBoostAreaComponent(float boostMultiplier = 1.5f, float durationSec = 3.0f);

	void OnAttach(Entity& owner) override;
	void Update(float deltaTime) override;
	void DrawInspectorImGui() override;

	void SetBoostMultiplier(float multiplier);
	void SetDurationSec(float durationSec);
	[[nodiscard]] float GetBoostMultiplier() const noexcept;
	[[nodiscard]] float GetDurationSec() const noexcept;

private:
	void CheckPlayerCollision();
	void ApplyBoost(MovementComponent& movement) const;

	AABBCollider* mCollider = nullptr;
	float         mBoostMultiplier = 1.5f;
	float         mDurationSec = 3.0f;
	bool          mWasInside = false;
};

