#include "IMovementMode.h"

namespace Unnamed {
	Vec3 IMovementMode::GetWishDirHoriz(MovementContext& context) const {
		const Vec3 right = context.input.right.Normalized();
		Vec3 forward = context.input.forward.Normalized();
		forward.y = 0.0f;
		forward.Normalize();

		Vec3 wishDir =
			right * context.input.moveAxis.x +
			forward * context.input.moveAxis.z;
		wishDir.y = 0.0f;
		return wishDir;
	}

	Vec3 IMovementMode::GetWishDir(MovementContext& context) const {
		const Vec3 right = context.input.right.Normalized();
		const Vec3 up = context.input.up.Normalized();
		const Vec3 forward = context.input.forward.Normalized();
		return right * context.input.moveAxis.x +
			up * context.input.moveAxis.y +
			forward * context.input.moveAxis.z;
	}
}
