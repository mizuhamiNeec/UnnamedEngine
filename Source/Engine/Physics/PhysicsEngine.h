#pragma once
#include <unordered_set>

#include <Entity/Base/Entity.h>

#include <Lib/Math/Vector/Vec3.h>

#include <Physics/Physics.h>

class ColliderComponent;

struct HitResult {
	bool isHit = false; // 衝突しているか?
	Vec3 hitPos = Vec3::zero; // 衝突した座標
	Vec3 hitNormal = Vec3::zero; // 衝突した面の法線
	float depth = 0.0f; // 衝突深度
	float dist = 0.0f; // 開始点からヒットした点までの距離
	Entity* hitEntity = nullptr; // 衝突したエンティティ
};

class PhysicsEngine {
public:
	PhysicsEngine() = default;

	void Init();
	void Update(float deltaTime);

	void RegisterEntity(Entity* entity, bool isStatic = false);
	void UnregisterEntity(Entity* entity);

	std::vector<HitResult> BoxCast(
		const Vec3& start,
		const Vec3& direction,
		float distance,
		const Vec3& halfSize
	);

private:
	static bool IntersectAABBWithTriangle(const AABB& aabb, const Triangle& triangle, HitResult& outHit);

	static Vec3 ClosestPointOnTriangleToPoint(const Vec3& point, const Triangle& triangle);
	static Vec3 ClosestPointOnAABBToPoint(const Vec3& point, const AABB& aabb);

	void UpdateBVH();

	struct StaticMeshData {
		StaticMeshData();
		StaticMeshData(StaticMeshData&& other) noexcept
			: worldTriangles(std::move(other.worldTriangles))
			, worldBounds(other.worldBounds)
			, localBVH(std::move(other.localBVH)) {
		}

		std::vector<Triangle> worldTriangles = {};
		AABB worldBounds = { Vec3::zero, Vec3::zero };
		DynamicBVH localBVH; // メッシュ内部の三角形用BVH
	};

	struct DynamicMeshData {
		std::vector<Triangle> localTriangles;
		const TransformComponent* transform = nullptr;
	};

	std::unordered_map<ColliderComponent*, StaticMeshData> staticMeshes_;
	std::unordered_map<ColliderComponent*, DynamicMeshData> dynamicMeshes_;

	std::unordered_map<ColliderComponent*, int> colliderNodeIds_; // コライダーとノードのIDマップ

	DynamicBVH staticBVH_; // 静的オブジェクト用BVH
	DynamicBVH dynamicBVH_; // 動的オブジェクト用BVH

	std::unordered_set<Entity*> registeredEntities_; // 登録されたエンティティを追跡
	std::vector<ColliderComponent*> colliderComponents_; // 登録されたコライダーコンポーネントのリスト
};
