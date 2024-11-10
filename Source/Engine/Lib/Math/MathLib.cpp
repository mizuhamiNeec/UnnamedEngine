#include "MathLib.h"
#include "../Lib/Structs/Structs.h"

bool Math::IsCollision(const AABB& aabb, const Vec3& point) {
	return (
		point.x >= aabb.min.x && point.x <= aabb.max.x &&
		point.y >= aabb.min.y && point.y <= aabb.max.y &&
		point.z >= aabb.min.z && point.z <= aabb.max.z
	);
}

Vec3 Math::Lerp(const Vec3& a, const Vec3& b, const float t) {
	return a * (1 - t) + b * t;
}
