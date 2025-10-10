#pragma once
#include <runtime/core/math/Math.h>

namespace Unnamed {
	struct Ray {
		Vec3  origin;         // Origin
		Vec3  dir;            // Direction
		Vec3  invDir;         // 1 / dir
		float tMin = 0.0f;    // Minimum distance
		float tMax = FLT_MAX; // Maximum distance
	};

	struct Line {
		Vec3 start = Vec3::zero;
		Vec3 end   = Vec3::right;
	};

	struct Triangle {
		Vec3 v0 = Vec3(-0.86603f, -0.5f, 0.0f);
		Vec3 v1 = Vec3(0.86603f, -0.5f, 0.0f);
		Vec3 v2 = Vec3::up;
		// 0.86603 = sin(60°)
	};

	struct Box {
		Vec3 center   = Vec3::zero;
		Vec3 halfSize = Vec3::one * 0.5f;
	};

	struct AABB {
		Vec3 min = Vec3(FLT_MAX);
		Vec3 max = Vec3(-FLT_MAX);

		void Expand(const Vec3& point);
		void Expand(const AABB& aabb);

		[[nodiscard]] Vec3 Center() const;

		[[nodiscard]] float SurfaceArea() const;
		[[nodiscard]] int   LongestAxis() const;

		[[nodiscard]] Vec3 Size() const;
	};

	struct Sphere {
		Vec3  center = Vec3::zero;
		float radius = 0.5f;
	};

	struct Capsule {
		Vec3  start  = Vec3::down * 0.5f;
		Vec3  end    = Vec3::up * 0.5f;
		float radius = 0.5f;
	};
}
