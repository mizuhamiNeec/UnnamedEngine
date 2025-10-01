#include <engine/public/uphysics/CollisionDetection.h>
#include <engine/public/uphysics/RayCast.h>

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
}
