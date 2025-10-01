#pragma once
#include <engine/public/uphysics/ShapeCast.h>

namespace UPhysics {
	struct BoxCast final : ShapeCast {
		[[nodiscard]] Unnamed::AABB ExpandNode(const Unnamed::AABB& nodeBounds) const override;

		bool TestTriangle(
			const Unnamed::Triangle& triangle,
			const Vec3&     dir,
			float           length,
			float&          outTOI,
			Vec3&           outNormal
		) const override;

		Unnamed::Box  box;
		Vec3 half = box.halfSize;
	};
}
