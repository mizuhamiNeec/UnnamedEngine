#pragma once
#include <engine/public/uphysics/Primitives.h>

namespace UPhysics {
	bool RayVsAABB(
		const Ray& ray, const AABB& aabb,
		float&     tMaxOut
	);

	bool TriangleVsRay(
		const Triangle& triangle, const Ray& ray,
		float&          tHit, Vec3&          outNormal
	);

	bool SweptAabbVsTriSAT(
		const Box&      box0,
		const Vec3&     delta, // 速度 * dt
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNrm
	);
}
