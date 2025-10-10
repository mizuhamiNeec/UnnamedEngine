#pragma once
#include <engine/uphysics/ShapeCast.h>

namespace UPhysics {
	struct RayCast final : ShapeCast {
		[[nodiscard]] Unnamed::AABB ExpandNode(const Unnamed::AABB& nodeBounds) const override;

		bool TestTriangle(
			const Unnamed::Triangle& tri,
			const Vec3&     dir,
			float           length,
			float&          toi,
			Vec3&           normal
		) const override;

		bool OverlapAtStart(
			const Unnamed::Triangle& tri,
			float&          depth,
			Vec3&           normal
		) const override;

		Vec3 start;
		Vec3 invDir;
	};
}
