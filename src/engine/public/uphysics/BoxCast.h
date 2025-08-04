#pragma once
#include <engine/public/uphysics/ShapeCast.h>

namespace UPhysics {
	struct BoxCast final : ShapeCast {
		[[nodiscard]] AABB ExpandNode(const AABB& nodeBounds) const override;

		bool TestTriangle(
			const Triangle& triangle,
			const Vec3&     dir,
			float           length,
			float&          outTOI,
			Vec3&           outNormal
		) const override;

		Box  box;
		Vec3 half = box.half;
	};
}
