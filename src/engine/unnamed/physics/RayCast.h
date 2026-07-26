#pragma once
#include <engine/unnamed/physics/ShapeCast.h>

namespace Unnamed::Physics {
	/// @brief レイと三角形群の最近接ヒットを評価します
	struct RayCast final : ShapeCast {
		[[nodiscard]] AABB ExpandNode(
			const AABB& nodeBounds
		) const override;

		bool TestTriangle(
			const Triangle& tri,
			const Vec3&     dir,
			float           length,
			float&          toi,
			Vec3&           normal
		) const override;

		bool OverlapAtStart(
			const Triangle& tri,
			float&          depth,
			Vec3&           normal
		) const override;

		Vec3 start;
		Vec3 invDir;
	};
}
