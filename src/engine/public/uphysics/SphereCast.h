#pragma once
#include "ShapeCast.h"

namespace UPhysics {
	struct SphereCast final : ShapeCast {
		[[nodiscard]] AABB ExpandNode(const AABB& nodeBounds) const override;

		bool TestTriangle(
			const Triangle& triangle,
			const Vec3&     dir,
			float           length,
			float&          outTOI,
			Vec3&           outNormal
		) const override;

		Vec3  center;
		float radius;
	};
}
