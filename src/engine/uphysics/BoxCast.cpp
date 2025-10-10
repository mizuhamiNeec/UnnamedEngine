#include <engine/uphysics/BoxCast.h>
#include <engine/uphysics/CollisionDetection.h>

#include <cmath>

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

	bool BoxCast::OverlapAtStart(
		const Unnamed::Triangle& triangle,
		float&          depth,
		Vec3&           normal
	) const {
		return BoxVsTriangleOverlap(box, triangle, normal, depth);
	}

	Vec3 BoxCast::ComputeImpactPoint(
		const Vec3& start,
		const Vec3& dirNormalized,
		float       length,
		float       toi,
		const Vec3& normal
	) const {
		const float travel = toi * length;
		Vec3        center = start + dirNormalized * travel;
		Vec3        n      = normal;
		const float nLenSq = n.SqrLength();
		if (nLenSq > 1e-12f) {
			n /= std::sqrt(nLenSq);
		} else {
			return center;
		}
		const Vec3 absN{std::fabs(n.x), std::fabs(n.y), std::fabs(n.z)};
		const float support = absN.x * half.x + absN.y * half.y + absN.z * half.z;
		return center - n * support;
	}
}
