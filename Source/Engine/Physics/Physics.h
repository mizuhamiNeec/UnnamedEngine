#pragma once
#include <shared_mutex>
#include <stack>
#include <vector>
#include <utility>

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

// Axis Aligned Bounding Box (軸に沿った境界ボックス)
struct AABB {
	Vec3 min;
	Vec3 max;
	AABB(const Vec3& min, const Vec3& max);
	[[nodiscard]] Vec3 GetCenter() const;
	[[nodiscard]] Vec3 GetSize() const;
	[[nodiscard]] Vec3 GetHalfSize() const;

	bool Intersects(const AABB& other) const;
	AABB Combine(const AABB& aabb) const;
	float Volume() const;
};

bool OverlapOnAxis(Vec3 aabbHalfSize, Vec3 v0, Vec3 v1, Vec3 v2, Vec3 axis);

AABB FromTriangles(const std::vector<Triangle>& vector);
AABB FromTriangle(const Triangle& triangle);

struct Capsule {
	Vec3 start;	  // 始点
	Vec3 end;	  // 終点
	float radius; // 半径
};

//-------------------------------------------------------------------------
// 大量のオブジェクトを出したいのでBVHを採用
//-------------------------------------------------------------------------
class DynamicBVH {
public:
	DynamicBVH() = default;
	DynamicBVH(DynamicBVH&& other) noexcept
		: nodes_(std::move(other.nodes_)), rootNode_(other.rootNode_), freeList_(std::move(other.freeList_)) {
		other.rootNode_ = -1;
	}

	void Clear();

	int InsertObject(const AABB& objectAABB, int objectIndex);
	void RemoveObject(int nodeId);
	void UpdateObject(int nodeId, const AABB& newAABB);
	void ReserveNodes(size_t capacity);

	[[nodiscard]] std::vector<int> QueryOverlaps(const AABB& queryBox) const;

	float CalculateGrowth(int nodeId, const AABB& newAABB) const;
	void CreateNewParent(int existingNodeId, int newNodeId);

	// デバッグ用
	void DrawBvhNode(int nodeId, const Vec4& color) const;
	void DrawBvh(const Vec4& color) const;
	void DrawObjects(const Vec4& color) const;

	[[nodiscard]] AABB GetNodeAABB(int nodeId) const;

private:
	struct BVHNode {
		AABB boundingBox = { Vec3::zero, Vec3::zero };
		int leftChild = -1;
		int rightChild = -1;
		int parent = -1;
		int objectIndex = -1;
		bool isLeaf = false;
		bool isValid = true; // 有効なノードか
	};

	std::vector<BVHNode> nodes_;
	int rootNode_ = -1;
	std::stack<int> freeList_;

	mutable std::shared_mutex bvhMutex_; // マルチスレッド対応

	// 新しいメンバ関数
	void OptimizeMemoryLayout();
};

namespace Physics {
	// ヘルパー関数
	bool RayIntersectsTriangle(const Vec3& rayOrigin, const Vec3& rayDir, const Triangle& triangle, float& outTime);
	float ComputeBoxPenetration(const Vec3& boxCenter, const Vec3& halfSize, const Vec3& hitPos, const Vec3& hitNormal);
	Vec3 ComputeAABBOverlap(const AABB& a, const AABB& b);
#pragma region BVH

#pragma endregion

	// #pragma region 関数
	//	void ProjectAABBOntoAxis(const Vec3 center, const Vec3 aabbHalfSize, Vec3 axis, float& outMin, float& outMax);
	//	void ProjectTriangleOntoAxis(const Triangle& triangle, const Vec3& axis, float& outMin, float& outMax);
	//	bool TestAxis(const Vec3& axis, const Vec3& aabbCenter, const Vec3& aabbHalfSize, const Triangle& triangle);
	//
	//	HitResult IntersectAABBWithTriangle(const AABB& aabb, const Triangle& triangle);
	//	void DebugDrawAABBAndTriangle(const AABB& aabb, const Triangle& triangle, const HitResult& result);
	//	Vec3 ResolveCollisionAABB(const AABB& aabb, Vec3& velocity, const std::vector<Triangle>& triangles, int maxIterations = 5);
	//	void DebugDrawAABBCollisionResponse(const AABB& aabb, const Vec3& velocity, const Vec3& finalPosition);
	//
	//	void ClosestPointBetweenSegments(const Vec3& segA1, const Vec3& segA2, const Vec3& segB1, const Vec3& segB2, Vec3& outSegA, Vec3& outSegB);
	//	Vec3 ClosestPointOnTriangle(const Vec3& point, const Triangle& triangle);
	//	HitResult IntersectCapsuleWithTriangle(const Capsule& capsule, const Triangle& triangle);
	//	void DebugDrawCapsuleAndTriangle(const Capsule& capsule, const Triangle& triangle, const HitResult& result);
	//
	// #pragma endregion
}
