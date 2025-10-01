#include <engine/public/uphysics/BoxCast.h>
#include <engine/public/uphysics/CollisionDetection.h>

namespace UPhysics {
	Unnamed::AABB BoxCast::ExpandNode(const Unnamed::AABB& nodeBounds) const {
		return Unnamed::AABB{
			nodeBounds.min - half,
			nodeBounds.max + half
		};
	}

	bool BoxCast::TestTriangle(
		const Unnamed::Triangle& triangle,
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
