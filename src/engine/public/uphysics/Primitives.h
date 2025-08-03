#pragma once

#include <math/public/MathLib.h>

namespace UPhysics {
	// 三角形
	struct Triangle {
		Vec3 v0, v1, v2;
	};

	struct Ray {
		Vec3  start;       // 始点
		Vec3  dir;         // 方向
		Vec3  invDir;      // 1/dir
		float tMin = 0.0f; // 最小距離
		float tMax = FLT_MAX;
	};

	struct Box {
		Vec3 center; // 中心
		Vec3 half;   // 半径（半分のサイズ）
	};

	// ヒット情報
	struct Hit {
		float    t = FLT_MAX;
		Vec3     pos;
		Vec3     normal;
		uint32_t triIndex = UINT_FAST32_MAX;
	};

	// AABB
	struct AABB {
		Vec3 min = Vec3(FLT_MAX, FLT_MAX, FLT_MAX);
		Vec3 max = Vec3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		void Expand(const Vec3& point);
		void Expand(const AABB& aabb);

		Vec3 Center() const;

		[[nodiscard]] float SurfaceArea() const;
		[[nodiscard]] int   LongestAxis() const;
	};

	// 形状情報
	struct TriInfo {
		AABB     bounds;   // 境界
		Vec3     center;   // 中心
		uint32_t triIndex; // 三角形のインデックス
	};
}
