#include <engine/public/uphysics/BoxCast.h>
#include <engine/public/uphysics/CollisionDetection.h>

namespace UPhysics {
	AABB BoxCast::ExpandNode(const AABB& nodeBounds) const {
		return AABB{
			nodeBounds.min - half,
			nodeBounds.max + half
		};
	}

	bool BoxCast::TestTriangle(
		const Triangle& triangle,
		const Vec3&     dir,
		const float     length,
		float&          outTOI,
		Vec3&           outNormal
	) const {
		return SweptAabbVsTriSAT(
			box,
			dir * length,
			triangle,
			outTOI,
			outNormal
		);
	}
}
