#pragma once
#include <engine/public/uphysics/PhysicsTypes.h>

namespace UPhysics {
	bool RayVsAABB(
		const Unnamed::Ray& ray, const Unnamed::AABB& aabb,
		float&              tMaxOut
	);

	bool TriangleVsRay(
		const Unnamed::Triangle& triangle, const Unnamed::Ray& ray,
		float&          tHit, Vec3&          outNormal
	);

	bool SweptAabbVsTriSAT(
		const Unnamed::Box&      box0,
		const Vec3&     delta, // 速度 * dt
		const Unnamed::Triangle& tri,
		float&          outTOI,
		Vec3&           outNrm
	);

	bool SweptSphereVsTriSAT(
		const Vec3&     center,
		float           radius,
		const Vec3&     delta,
		const Unnamed::Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	bool SweptCylinderVsTriSAT(
		const Vec3&     base,
		float           halfHeight,
		float           radius,
		const Vec3&     delta,
		const Unnamed::Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	bool SweptCapsuleVsTriSAT(
		const Vec3&     a,
		const Vec3&     b,
		float           radius,
		const Vec3&     delta,
		const Unnamed::Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	);

	// Overlap detection functions
	bool BoxVsTriangleOverlap(
		const Unnamed::Box&      box,
		const Unnamed::Triangle& tri,
		Vec3&           outSeparationAxis,
		float&          outPenetrationDepth
	);
}
