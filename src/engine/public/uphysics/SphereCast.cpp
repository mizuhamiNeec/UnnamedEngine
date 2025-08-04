#include "SphereCast.h"

#include "CollisionDetection.h"

namespace UPhysics {
	AABB SphereCast::ExpandNode(const AABB& nodeBounds) const {
		const Vec3 r = Vec3(radius);
		// わずかなマージンを追加して数値誤差を防ぐ
		const Vec3 margin = Vec3(1e-6f);
		return {
			nodeBounds.min - r - margin,
			nodeBounds.max + r + margin
		};
	}

	bool SphereCast::TestTriangle(
		const Triangle& triangle,
		const Vec3&     dir,
		float           length,
		float&          outTOI,
		Vec3&           outNormal
	) const {
		return SweptSphereVsTriSAT(
			center,
			radius,
			dir * length,
			triangle,
			outTOI,
			outNormal
		);
	}
}
