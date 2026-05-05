#pragma once

#include "../interface/IMovementMode.h"

namespace Unnamed {
	/// @brief デバッグ用のノークリップ移動Modeです。
	class NoclipMovementMode final : public IMovementMode {
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
			Vec3 wishDir,
			float wishSpeed,
			float accel,
			float deltaTime
		) const;
	};
}
