#pragma once

#include "../interface/IMovementMode.h"

namespace Unnamed {
	namespace Physics {
		struct Hit;
	}

	/// @brief 基本の地上移動Modeです。
	class GroundMovementMode final : public IMovementMode {
	public:
		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;

		[[nodiscard]] MOVEMENT_MODE_ID GetModeId() const override;
		[[nodiscard]] std::string_view GetModeName() const override;

	private:
		void ApplyFriction(Vec3& velocity, float amount, float deltaTime) const;
		void Accelerate(
			Vec3& currentVel,
			Vec3  wishDir,
			float wishSpeed,
			float accel,
			float deltaTime
		) const;
		[[nodiscard]] bool IsGrounded(
			const BaseKinematicCollisionResolver* resolver,
			const Vec3&                           position,
			Physics::Hit*                         outHit = nullptr
		) const;
	};
}
