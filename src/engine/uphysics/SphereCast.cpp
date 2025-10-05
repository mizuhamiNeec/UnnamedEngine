#include "SphereCast.h"

#include "CollisionDetection.h"

#include <cmath>

namespace {
	static inline Vec3 ClosestPointOnSegment(const Vec3& a, const Vec3& b, const Vec3& p) {
		const Vec3 ab = b - a;
		const float abLen2 = ab.Dot(ab);
		if (abLen2 <= 1e-12f) return a;
		const float t = std::clamp((p - a).Dot(ab) / abLen2, 0.0f, 1.0f);
		return a + ab * t;
	}

	static inline Vec3 ClosestPointOnTriangle(const Unnamed::Triangle& tri, const Vec3& p) {
		// Barycentric technique
		const Vec3 a = tri.v0;
		const Vec3 b = tri.v1;
		const Vec3 c = tri.v2;
		// Check against vertices/edges
		const Vec3 ab = b - a;
		const Vec3 ac = c - a;
		const Vec3 ap = p - a;
		const float d1 = ab.Dot(ap);
		const float d2 = ac.Dot(ap);
		if (d1 <= 0.0f && d2 <= 0.0f) return a;

		const Vec3 bp = p - b;
		const float d3 = ab.Dot(bp);
		const float d4 = ac.Dot(bp);
		if (d3 >= 0.0f && d4 <= d3) return b;

		const float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) {
			const float v = d1 / (d1 - d3);
			return a + ab * v;
		}

		const Vec3 cp = p - c;
		const float d5 = ab.Dot(cp);
		const float d6 = ac.Dot(cp);
		if (d6 >= 0.0f && d5 <= d6) return c;

		const float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) {
			const float w = d2 / (d2 - d6);
			return a + ac * w;
		}

		const float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && (d4 - d3) >= 0.0f && (d5 - d6) >= 0.0f) {
			const Vec3 bc = c - b;
			const float t = std::clamp((p - b).Dot(bc) / bc.Dot(bc), 0.0f, 1.0f);
			return b + bc * t;
		}

		// Inside face region
		const Vec3 n = (ab.Cross(ac)).Normalized();
		const float dist = (p - a).Dot(n);
		return p - n * dist;
	}
}

namespace UPhysics {
	Unnamed::AABB SphereCast::ExpandNode(const Unnamed::AABB& nodeBounds) const {
		const Vec3 r = Vec3(radius);
		// わずかなマージンを追加して数値誤差を防ぐ
		const Vec3 margin = Vec3(1e-6f);
		return {
			nodeBounds.min - r - margin,
			nodeBounds.max + r + margin
		};
	}

	bool SphereCast::TestTriangle(
		const Unnamed::Triangle& triangle,
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

	bool SphereCast::OverlapAtStart(
		const Unnamed::Triangle& triangle,
		float& depth,
		Vec3& normal
	) const {
		const Vec3 q = ClosestPointOnTriangle(triangle, center);
		const Vec3 v = center - q;
		const float dist = v.Length();
		if (dist < radius) {
			depth = radius - dist;
			normal = (dist > 1e-8f) ? (v / dist) : ((triangle.v1 - triangle.v0).Cross(triangle.v2 - triangle.v0).Normalized());
			return true;
		}
		return false;
	}


	Vec3 SphereCast::ComputeImpactPoint(
		const Vec3& start,
		const Vec3& dirNormalized,
		float       length,
		float       toi,
		const Vec3& normal
	) const {
		const float travel       = toi * length;
		Vec3        impactCenter = start + dirNormalized * travel;
		Vec3        n            = normal;
		const float nLenSq = n.SqrLength();
		if (nLenSq > 1e-12f) {
			n /= std::sqrt(nLenSq);
		} else {
			return impactCenter;
		}
		return impactCenter - n * radius;
	}
}
