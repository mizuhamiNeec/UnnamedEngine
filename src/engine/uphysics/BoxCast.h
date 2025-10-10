#pragma once
#include <engine/uphysics/ShapeCast.h>

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

		bool OverlapAtStart(
			const Unnamed::Triangle& triangle,
			float&          depth,
			Vec3&           normal
		) const override;

		Vec3 ComputeImpactPoint(
			const Vec3& start,
			const Vec3& dirNormalized,
			float       length,
			float       toi,
			const Vec3& normal
		) const override;

		Unnamed::Box  box;
		Vec3 half = box.halfSize;
	};
}
