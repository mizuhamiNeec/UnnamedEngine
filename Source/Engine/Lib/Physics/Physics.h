#pragma once
#undef min
#undef max
#include <Lib/Math/Vector/Vec3.h>

#include "Debug/Debug.h"

namespace Physics {
#pragma region 構造体
	struct CollisionResult {
		bool hasCollision = false; // 衝突しているか?
		Vec3 contactPoint; // 衝突点
		Vec3 contactNormal; // 衝突面の法線ベクトル
		float distance = 0.0f; // 衝突点までの距離
	};

	// Axis Aligned Bounding Box (軸に沿った境界ボックス)
	struct AABB {
		Vec3 min;
		Vec3 max;
		AABB(const Vec3& min, const Vec3& max);
		[[nodiscard]] Vec3 GetCenter() const;
		[[nodiscard]] Vec3 GetSize() const;
		[[nodiscard]] Vec3 GetHalfSize() const;
	};

	// 三角
	struct Triangle {
		Vec3 v0;
		Vec3 v1;
		Vec3 v2;
		Triangle(const Vec3& v0, const Vec3& v1, const Vec3& v2);
		[[nodiscard]] Vec3 GetNormal() const;
		[[nodiscard]] float GetArea() const;
		[[nodiscard]] Vec3 GetCenter() const;
		bool IsPointInside(const Vec3& point) const;
		Vec3 GetVertex(int index) const;
	};

	struct Capsule {
		Vec3 start; // 始点
		Vec3 end; // 終点
		float radius; // 半径
	};
#pragma endregion

#pragma region 関数
	void ProjectAABBOntoAxis(const Vec3 center, const Vec3 aabbHalfSize, Vec3 axis, float& outMin, float& outMax);
	void ProjectTriangleOntoAxis(const Triangle& triangle, const Vec3& axis, float& outMin, float& outMax);
	bool TestAxis(const Vec3& axis, const Vec3& aabbCenter, const Vec3& aabbHalfSize, const Triangle& triangle);

	CollisionResult IntersectAABBWithTriangle(const AABB& aabb, const Triangle& triangle);
	void DebugDrawAABBAndTriangle(const AABB& aabb, const Triangle& triangle, const CollisionResult& result);
	Vec3 ResolveCollisionAABB(const AABB& aabb, Vec3& velocity, const std::vector<Triangle>& triangles, int maxIterations = 5);
	void DebugDrawAABBCollisionResponse(const AABB& aabb, const Vec3& velocity, const Vec3& finalPosition);

	void ClosestPointBetweenSegments(const Vec3& segA1, const Vec3& segA2, const Vec3& segB1, const Vec3& segB2, Vec3& outSegA, Vec3& outSegB);
	Vec3 ClosestPointOnTriangle(const Vec3& point, const Triangle& triangle);
	CollisionResult IntersectCapsuleWithTriangle(const Capsule& capsule, const Triangle& triangle);
	void DebugDrawCapsuleAndTriangle(const Capsule& capsule, const Triangle& triangle, const CollisionResult& result);

#pragma endregion
}
