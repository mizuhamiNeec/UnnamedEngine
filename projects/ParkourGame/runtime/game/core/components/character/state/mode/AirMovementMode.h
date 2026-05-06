#pragma once

#include "../interface/IMovementMode.h"

namespace Unnamed {
	namespace Physics {
		struct Hit;
	}

	/// @brief 基本の空中移動Modeです。
	class AirMovementMode final : public IMovementMode {
	public:
		void Enter(ConsoleSystem* console) override;
		void Tick(MovementContext& context, float deltaTime) override;
		void Exit() override;

		[[nodiscard]] MOVEMENT_MODE_ID GetModeId() const override;
		[[nodiscard]] std::string_view GetModeName() const override;

	private:
		void AirAccelerate(
			Vec3& currentVel,
			Vec3 wishDir,
			float wishSpeed,
			float accel,
			float deltaTime
		) const;

		void ApplyHalfGravity(Vec3& target, float deltaTime) const;

		[[nodiscard]] bool IsGrounded(
			const BaseKinematicCollisionResolver* resolver,
			const Vec3& position,
			Physics::Hit* outHit = nullptr
		) const;
	};
}
