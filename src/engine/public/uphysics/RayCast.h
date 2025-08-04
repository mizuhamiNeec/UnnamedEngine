#pragma once
#include <engine/public/uphysics/ShapeCast.h>

namespace UPhysics {
	struct RayCast final : ShapeCast {
		[[nodiscard]] AABB ExpandNode(const AABB& nodeBounds) const override;

		bool TestTriangle(
			const Triangle& tri,
			const Vec3&     dir,
			float           length,
			float&          toi,
			Vec3&           normal
		) const override;

		Vec3 start;
		Vec3 invDir;
	};
}
