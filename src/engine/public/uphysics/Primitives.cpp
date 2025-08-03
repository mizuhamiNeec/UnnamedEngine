#include <engine/public/uphysics/Primitives.h>

namespace UPhysics {
#pragma region AABB
	void AABB::Expand(const Vec3& point) {
		min = Vec3::Min(min, point);
		max = Vec3::Max(max, point);
	}

	void AABB::Expand(const AABB& aabb) {
		Expand(aabb.min);
		Expand(aabb.max);
	}

	Vec3 AABB::Center() const {
		return min + max * 0.5f;
	}

	float AABB::SurfaceArea() const {
		const Vec3 d = max - min;
		return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
	}

	int AABB::LongestAxis() const {
		const Vec3 d = max - min;
		if (d.x >= d.y && d.x >= d.z) {
			return 0; // X軸
		}
		if (d.y >= d.x && d.y >= d.z) {
			return 1; // Y軸
		}
		return 2; // Z軸
	}
#pragma endregion
}
