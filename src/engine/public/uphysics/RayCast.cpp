#include <engine/public/uphysics/CollisionDetection.h>
#include <engine/public/uphysics/RayCast.h>

namespace UPhysics {
	AABB RayCast::ExpandNode(const AABB& nodeBounds) const {
		return nodeBounds;
	}

	bool RayCast::TestTriangle(
		const Triangle& tri,
		const Vec3&     dir,
		const float     length,
		float&          toi,
		Vec3&           normal
	) const {
		float     t   = length;
		const Ray ray = {
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
}
