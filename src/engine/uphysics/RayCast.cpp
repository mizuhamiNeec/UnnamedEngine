#include <engine/uphysics/CollisionDetection.h>
#include <engine/uphysics/RayCast.h>

namespace UPhysics {
	Unnamed::AABB RayCast::ExpandNode(const Unnamed::AABB& nodeBounds) const {
		return nodeBounds;
	}

	bool RayCast::TestTriangle(
		const Unnamed::Triangle& tri,
		const Vec3&     dir,
		const float     length,
		float&          toi,
		Vec3&           normal
	) const {
		float     t   = length;
		const Unnamed::Ray ray = {
			start,
			dir,
			invDir,
			0.0f,
			length
		};
		if (!TriangleVsRay(tri, ray, t, normal)) {
			return false; // 交差しなかった
		}
		toi = t / length;
		return true;
	}

	bool RayCast::OverlapAtStart(
		const Unnamed::Triangle& /*tri*/,
		float& /*depth*/,
		Vec3& /*normal*/
	) const {
		// レイは体積を持たないため、開始時に"重なっている"概念は扱わない
		return false;
	}
}
