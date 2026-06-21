#include "Physics.h"

#include <algorithm>
#include <chrono>
#include <pch.h>
#include <vector>

#include <engine/unnamed/physics/BoxCast.h>
#include <engine/unnamed/physics/PhysicsTypes.h>
#include <engine/unnamed/physics/RayCast.h>
#include <engine/unnamed/physics/SphereCast.h>
#include <engine/unnamed/subsystem/console/Log.h>

namespace Unnamed::Physics {
	namespace {
		[[nodiscard]] Triangle BuildTriangleLocal(
			const MeshVertex& a,
			const MeshVertex& b,
			const MeshVertex& c
		) {
			return {
				.v0 = a.position,
				.v1 = b.position,
				.v2 = c.position,
			};
		}

		[[nodiscard]] Triangle BuildTriangleWorld(
			const MeshVertex& a,
			const MeshVertex& b,
			const MeshVertex& c,
			const Mat4&       world
		) {
			return {
				.v0 = world.TransformPoint(a.position),
				.v1 = world.TransformPoint(b.position),
				.v2 = world.TransformPoint(c.position),
			};
		}

		[[nodiscard]] bool IsAABBOverlap(
			const AABB& lhs,
			const AABB& rhs
		) {
			return lhs.max.x >= rhs.min.x && lhs.min.x <= rhs.max.x &&
			       lhs.max.y >= rhs.min.y && lhs.min.y <= rhs.max.y &&
			       lhs.max.z >= rhs.min.z && lhs.min.z <= rhs.max.z;
		}
	}

	/// @brief 初期化
	void Engine::Init() {
	}

	/// @brief 更新
	void Engine::Update(float) const {
	}

	void Engine::EndFrame() const {
#ifdef _DEBUG
		mDebugBox.clear();
#endif
	}

	/// @brief レイキャストを行う関数
	/// @param ray レイ情報
	/// @param outHit 衝突情報の出力先
	/// @return 衝突した場合trueを返す
	bool Engine::RayCast(const Ray& ray, Hit* outHit) const {
		Physics::RayCast cast;
		cast.start  = ray.origin;
		cast.invDir = ray.invDir;

		Hit  bestHit{};
		bool hasHit = false;
#ifdef _DEBUG
		std::vector<Box> bestDebugBoxes;
#endif

		const auto testSet = [&](const std::vector<RegisteredBVH>& set) {
			Hit hit{};
#ifdef _DEBUG
			std::vector<Box>  debugBoxes;
			std::vector<Box>* debugTarget = &debugBoxes;
#else
			std::vector<Box>* debugTarget = nullptr;
#endif
			if (!CastBVH(
				cast,
				ray.origin,
				ray.dir,
				ray.tMax,
				&hit,
				set,
				debugTarget
			)) {
				return;
			}
			if (!hasHit || hit.toi < bestHit.toi) {
				bestHit = hit;
				hasHit  = true;
#ifdef _DEBUG
				bestDebugBoxes = std::move(debugBoxes);
#endif
			}
		};

		testSet(mStaticBVHs);
		testSet(mDynamicBVHs);

#ifdef _DEBUG
		mDebugBox = hasHit ? std::move(bestDebugBoxes) : std::vector<Box>{};
#endif
		if (hasHit && outHit) {
			*outHit = bestHit;
		}
		return hasHit;
	}

	/// @brief ボックスキャストを行う関数
	/// @param box ボックス情報
	/// @param dir キャスト方向
	/// @param length キャスト距離
	/// @param outHit 衝突情報の出力先
	/// @return 衝突した場合trueを返す
	bool Engine::BoxCast(
		const Box&  box,
		const Vec3& dir,
		const float length,
		Hit*        outHit
	) const {
		Vec3  dirN = dir;
		float len  = length;

		const float dirLen = dirN.Length();
		if (dirLen > 1e-6f) {
			dirN /= dirLen;
			if (fabs(len - dirLen) < 1e-4f) {
				len = dirLen;
			}
		} else {
			return false;
		}

		Physics::BoxCast caster;
		caster.box  = box;
		caster.half = box.halfSize;

		Hit  bestHit{};
		bool hasHit = false;
#ifdef _DEBUG
		std::vector<Box> bestDebugBoxes;
#endif
		const auto testSet = [&](const std::vector<RegisteredBVH>& set) {
			Hit hit{};
#ifdef _DEBUG
			std::vector<Box>  debugBoxes;
			std::vector<Box>* debugTarget = &debugBoxes;
#else
			std::vector<Box>* debugTarget = nullptr;
#endif
			if (!CastBVH(
				caster,
				box.center,
				dirN,
				len,
				&hit,
				set,
				debugTarget
			)) {
				return;
			}
			if (!hasHit || hit.toi < bestHit.toi) {
				bestHit = hit;
				hasHit  = true;
#ifdef _DEBUG
				bestDebugBoxes = std::move(debugBoxes);
#endif
			}
		};

		testSet(mStaticBVHs);
		testSet(mDynamicBVHs);

#ifdef _DEBUG
		mDebugBox = hasHit ? std::move(bestDebugBoxes) : std::vector<Box>{};
#endif
		if (hasHit && outHit) {
			*outHit = bestHit;
		}
		return hasHit;
	}

	/// @brief スフィアキャストを行う関数
	/// @param start スフィアの開始位置
	/// @param radius スフィアの半径
	/// @param dir キャスト方向
	/// @param length キャスト距離
	/// @param outHit 衝突情報の出力先
	/// @return 衝突した場合trueを返す
	bool Engine::SphereCast(
		const Vec3& start,
		const float radius,
		const Vec3& dir,
		const float length,
		Hit*        outHit
	) const {
		if (radius <= 0.0f) {
			return false;
		}

		Vec3  dirN = dir;
		float len  = length;

		const float dirLen = dirN.Length();
		if (dirLen > 1e-6f) {
			dirN /= dirLen;
			if (fabs(len - dirLen) < 1e-4f) {
				len = dirLen;
			}
		} else {
			return false;
		}

		Physics::SphereCast cast;
		cast.center = start;
		cast.radius = radius;

		Hit  bestHit{};
		bool hasHit = false;
#ifdef _DEBUG
		std::vector<Box> bestDebugBoxes;
#endif
		const auto testSet = [&](const std::vector<RegisteredBVH>& set) {
			Hit hit{};
#ifdef _DEBUG
			std::vector<Box>  debugBoxes;
			std::vector<Box>* debugTarget = &debugBoxes;
#else
			std::vector<Box>* debugTarget = nullptr;
#endif
			if (!CastBVH(cast, start, dirN, len, &hit, set, debugTarget)) {
				return;
			}
			if (!hasHit || hit.toi < bestHit.toi) {
				bestHit = hit;
				hasHit  = true;
#ifdef _DEBUG
				bestDebugBoxes = std::move(debugBoxes);
#endif
			}
		};

		testSet(mStaticBVHs);
		testSet(mDynamicBVHs);

#ifdef _DEBUG
		mDebugBox = hasHit ? std::move(bestDebugBoxes) : std::vector<Box>{};
#endif
		if (hasHit && outHit) {
			*outHit = bestHit;
		}
		return hasHit;
	}

	/// @brief ボックスとメッシュの重なり判定を行う関数
	/// @param box ボックス情報
	/// @param outHit 衝突情報の出力先
	/// @return 重なりがあった場合trueを返す
	bool Engine::BoxOverlap(
		const Box& box,
		Hit*       outHit
	) const {
		if (mStaticBVHs.empty() && mDynamicBVHs.empty()) {
			return false;
		}

		AABB boxAABB;
		boxAABB.min = box.center - box.halfSize;
		boxAABB.max = box.center + box.halfSize;

		float bestPenetration = -FLT_MAX;
		Hit   bestHit{};
		bool  hasHit = false;

		const auto testSet = [&](const std::vector<RegisteredBVH>& bvhSet) {
			if (bvhSet.empty()) {
				return;
			}

			std::vector<const RegisteredBVH*> filtered;
			filtered.reserve(bvhSet.size());

			for (const auto& bvh : bvhSet) {
				if (bvh.nodes.empty() || bvh.triangles.empty()) {
					continue;
				}
				const AABB rootBounds = ToWorldBounds(bvh, bvh.nodes[0].bounds);
				if (IsAABBOverlap(boxAABB, rootBounds)) {
					filtered.emplace_back(&bvh);
				}
			}
			if (filtered.size() > 1) {
				std::sort(
					filtered.begin(),
					filtered.end(),
					[](const RegisteredBVH* lhs, const RegisteredBVH* rhs) {
						return lhs->ownerGuid < rhs->ownerGuid;
					}
				);
			}

			uint32_t stack[64];
			for (auto* bvh : filtered) {
				int sp      = 0;
				stack[sp++] = 0;

				while (sp) {
					const uint32_t index = stack[--sp];
					const auto& node = bvh->nodes[index];
					const AABB nodeBounds = ToWorldBounds(*bvh, node.bounds);
					if (!IsAABBOverlap(boxAABB, nodeBounds)) {
						continue;
					}

					if (node.primCount == 0) {
						stack[sp++] = node.leftFirst;
						stack[sp++] = node.rightFirst;
					} else {
						const uint32_t first = node.leftFirst;
						for (uint32_t i = 0; i < node.primCount; ++i) {
							const uint32_t triIdx = bvh->triIndices[first + i];
							const Triangle tri = ToWorldTriangle(*bvh, triIdx);

							Vec3  separationAxis;
							float penetrationDepth;
							if (!BoxVsTriangleOverlap(
								box,
								tri,
								separationAxis,
								penetrationDepth
							)) {
								continue;
							}

							const bool betterPenetration = penetrationDepth >
								bestPenetration;
							const bool equalPenetration = std::abs(
									penetrationDepth - bestPenetration
								) <= 1.0e-6f;
							const bool betterTieBreak = equalPenetration &&
								(bvh->ownerGuid < bestHit.hitEntityGuid ||
								 (bvh->ownerGuid == bestHit.hitEntityGuid &&
								  triIdx < bestHit.triIndex));
							if (!betterPenetration && !betterTieBreak) {
								continue;
							}

							bestPenetration = penetrationDepth;
							bestHit.toi     = 1.0f;
							bestHit.depth   = penetrationDepth;
							bestHit.pos     = box.center + separationAxis * (
								              std::min(
									              {
										              box.halfSize.x,
										              box.halfSize.y,
										              box.halfSize.z
									              }
								              ) - penetrationDepth * 0.5f);
							bestHit.normal        = separationAxis;
							bestHit.triIndex      = triIdx;
							bestHit.hitEntityGuid = bvh->ownerGuid;
							hasHit                = true;
						}
					}
				}
			}
		};

		testSet(mStaticBVHs);
		testSet(mDynamicBVHs);

		if (!hasHit) {
			return false;
		}
		if (outHit) {
			*outHit = bestHit;
		}
		return true;
	}

	int Engine::BoxOverlap(
		const Box& box,
		Hit*       outHits,
		const int  maxHits
	) const {
		if (maxHits <= 0 || outHits == nullptr) {
			return 0;
		}

		const int        collectorCap = std::clamp(maxHits * 4, 128, 1024);
		std::vector<Hit> collected(
			static_cast<size_t>(collectorCap)
		);

		int collectedCount = 0;
		collectedCount     += BoxOverlapSet(
			box,
			collected.data() + collectedCount,
			collectorCap - collectedCount,
			mStaticBVHs
		);
		if (collectedCount < collectorCap) {
			collectedCount += BoxOverlapSet(
				box,
				collected.data() + collectedCount,
				collectorCap - collectedCount,
				mDynamicBVHs
			);
		}

		if (collectedCount <= 0) {
			return 0;
		}
		if (collectedCount > 1) {
			std::sort(
				collected.begin(),
				collected.begin() + collectedCount,
				[](const Hit& lhs, const Hit& rhs) {
					if (lhs.hitEntityGuid != rhs.hitEntityGuid) {
						return lhs.hitEntityGuid < rhs.hitEntityGuid;
					}
					if (lhs.depth != rhs.depth) {
						return lhs.depth > rhs.depth;
					}
					if (lhs.normal.y != rhs.normal.y) {
						return lhs.normal.y > rhs.normal.y;
					}
					if (lhs.triIndex != rhs.triIndex) {
						return lhs.triIndex < rhs.triIndex;
					}
					return false;
				}
			);
		}

		const int outCount = std::min(maxHits, collectedCount);
		std::copy_n(collected.data(), outCount, outHits);
		return outCount;
	}

	int Engine::BoxOverlapRaw(
		const Box& box,
		Hit*       outHits,
		const int  maxHits
	) const {
		if (maxHits <= 0 || outHits == nullptr) {
			return 0;
		}

		// 生ヒットは法線の集合を保持したいため、収集バッファを大きめに確保
		const int        collectorCap = std::clamp(maxHits * 12, 256, 4096);
		std::vector<Hit> collected(
			static_cast<size_t>(collectorCap)
		);

		int collectedCount = 0;
		collectedCount     += BoxOverlapSetRaw(
			box,
			collected.data() + collectedCount,
			collectorCap - collectedCount,
			mStaticBVHs
		);
		if (collectedCount < collectorCap) {
			collectedCount += BoxOverlapSetRaw(
				box,
				collected.data() + collectedCount,
				collectorCap - collectedCount,
				mDynamicBVHs
			);
		}

		if (collectedCount <= 0) {
			return 0;
		}
		if (collectedCount > 1) {
			std::sort(
				collected.begin(),
				collected.begin() + collectedCount,
				[](const Hit& lhs, const Hit& rhs) {
					if (lhs.depth != rhs.depth) {
						return lhs.depth > rhs.depth;
					}
					if (lhs.hitEntityGuid != rhs.hitEntityGuid) {
						return lhs.hitEntityGuid < rhs.hitEntityGuid;
					}
					if (lhs.triIndex != rhs.triIndex) {
						return lhs.triIndex < rhs.triIndex;
					}
					if (lhs.normal.x != rhs.normal.x) {
						return lhs.normal.x < rhs.normal.x;
					}
					if (lhs.normal.y != rhs.normal.y) {
						return lhs.normal.y < rhs.normal.y;
					}
					if (lhs.normal.z != rhs.normal.z) {
						return lhs.normal.z < rhs.normal.z;
					}
					return false;
				}
			);
		}

		const int outCount = std::min(maxHits, collectedCount);
		std::copy_n(collected.data(), outCount, outHits);
		return outCount;
	}

	// --- Static/Dynamic mesh registration ---
	void Engine::ClearStaticMeshes() {
		mStaticBVHs.clear();
	}

	bool Engine::RegisterStaticMesh(
		const uint64_t                    ownerGuid,
		const std::span<const MeshVertex> vertices,
		const std::span<const uint32_t>   indices,
		const Mat4&                       world
	) {
		if (ownerGuid == 0 || vertices.empty() || indices.size() < 3) {
			return false;
		}

		RemoveColliderByOwnerGuid(mStaticBVHs, ownerGuid);
		RemoveColliderByOwnerGuid(mDynamicBVHs, ownerGuid);

		RegisteredBVH registered = BuildRegisteredMesh(
			ownerGuid,
			vertices,
			indices,
			world,
			ColliderMobility::Static
		);
		if (registered.ownerGuid == 0 || registered.triangles.empty() ||
		    registered.nodes.empty()) {
			return false;
		}

		mStaticBVHs.emplace_back(std::move(registered));
		return true;
	}

	bool Engine::RegisterDynamicMesh(
		const uint64_t                    ownerGuid,
		const std::span<const MeshVertex> vertices,
		const std::span<const uint32_t>   indices,
		const Mat4&                       world
	) {
		if (ownerGuid == 0 || vertices.empty() || indices.size() < 3) {
			return false;
		}

		RemoveColliderByOwnerGuid(mStaticBVHs, ownerGuid);
		RemoveColliderByOwnerGuid(mDynamicBVHs, ownerGuid);

		RegisteredBVH registered = BuildRegisteredMesh(
			ownerGuid,
			vertices,
			indices,
			world,
			ColliderMobility::Dynamic
		);
		if (registered.ownerGuid == 0 || registered.triangles.empty() ||
		    registered.nodes.empty()) {
			return false;
		}

		mDynamicBVHs.emplace_back(std::move(registered));
		return true;
	}

	bool Engine::UpdateDynamicMeshTransform(
		const uint64_t ownerGuid,
		const Mat4&    world
	) {
		if (ownerGuid == 0) {
			return false;
		}
		const auto it = std::find_if(
			mDynamicBVHs.begin(),
			mDynamicBVHs.end(),
			[ownerGuid](const RegisteredBVH& bvh) {
				return bvh.ownerGuid == ownerGuid;
			}
		);
		if (it == mDynamicBVHs.end()) {
			return false;
		}
		it->world = world;
		return true;
	}

	void Engine::UnregisterStaticMesh(const uint64_t ownerGuid) {
		if (ownerGuid == 0) {
			return;
		}
		RemoveColliderByOwnerGuid(mStaticBVHs, ownerGuid);
		RemoveColliderByOwnerGuid(mDynamicBVHs, ownerGuid);
	}

	void Engine::UnregisterDynamicMesh(const uint64_t ownerGuid) {
		if (ownerGuid == 0) {
			return;
		}
		RemoveColliderByOwnerGuid(mDynamicBVHs, ownerGuid);
	}

	std::vector<Box> Engine::GetDebugBVHBoxes() const {
		return mDebugBox;
	}

	AABB Engine::ToWorldBounds(
		const RegisteredBVH& bvh, const AABB& localBounds
	) {
		if (bvh.mobility == ColliderMobility::Dynamic) {
			const Vec3 corners[8]{
				{localBounds.min.x, localBounds.min.y, localBounds.min.z},
				{localBounds.max.x, localBounds.min.y, localBounds.min.z},
				{localBounds.min.x, localBounds.max.y, localBounds.min.z},
				{localBounds.max.x, localBounds.max.y, localBounds.min.z},
				{localBounds.min.x, localBounds.min.y, localBounds.max.z},
				{localBounds.max.x, localBounds.min.y, localBounds.max.z},
				{localBounds.min.x, localBounds.max.y, localBounds.max.z},
				{localBounds.max.x, localBounds.max.y, localBounds.max.z},
			};
			AABB worldBounds;
			for (const Vec3& p : corners) {
				worldBounds.Expand(bvh.world.TransformPoint(p));
			}
			return worldBounds;
		}
		return localBounds;
	}

	Triangle Engine::ToWorldTriangle(
		const RegisteredBVH& bvh, const uint32_t triIdx
	) {
		const Triangle& tri = bvh.triangles[triIdx];
		if (bvh.mobility == ColliderMobility::Dynamic) {
			return {
				.v0 = bvh.world.TransformPoint(tri.v0),
				.v1 = bvh.world.TransformPoint(tri.v1),
				.v2 = bvh.world.TransformPoint(tri.v2),
			};
		}
		return tri;
	}

	bool Engine::RemoveColliderByOwnerGuid(
		std::vector<RegisteredBVH>& bvhSet,
		const uint64_t              ownerGuid
	) {
		const auto oldSize = bvhSet.size();
		std::erase_if(
			bvhSet,
			[ownerGuid](const RegisteredBVH& bvh) {
				return bvh.ownerGuid == ownerGuid;
			}
		);
		return bvhSet.size() != oldSize;
	}

	RegisteredBVH Engine::BuildRegisteredMesh(
		const uint64_t                    ownerGuid,
		const std::span<const MeshVertex> vertices,
		const std::span<const uint32_t>   indices,
		const Mat4&                       world,
		const ColliderMobility            mobility
	) {
		RegisteredBVH registered{};
		if (ownerGuid == 0 || vertices.empty() || indices.size() < 3) {
			return registered;
		}

		const auto start = std::chrono::steady_clock::now();

		std::vector<Triangle> localTriangles;
		localTriangles.reserve(indices.size() / 3);

		const auto buildTrisStart = std::chrono::steady_clock::now();
		for (size_t i = 0; i + 2 < indices.size(); i += 3) {
			const uint32_t i0 = indices[i + 0];
			const uint32_t i1 = indices[i + 1];
			const uint32_t i2 = indices[i + 2];
			if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices
			    .size()) {
				continue;
			}

			const Triangle tri = mobility == ColliderMobility::Dynamic ?
				                     BuildTriangleLocal(
					                     vertices[i0],
					                     vertices[i1],
					                     vertices[i2]
				                     ) :
				                     BuildTriangleWorld(
					                     vertices[i0],
					                     vertices[i1],
					                     vertices[i2],
					                     world
				                     );

			const Vec3 edge01 = tri.v1 - tri.v0;
			const Vec3 edge02 = tri.v2 - tri.v0;
			if (edge01.Cross(edge02).SqrLength() <= 1e-12f) {
				continue;
			}
			localTriangles.emplace_back(tri);
		}
		const auto buildTrisEnd = std::chrono::steady_clock::now();

		if (localTriangles.empty()) {
			return registered;
		}

		const auto            bvhStart = std::chrono::steady_clock::now();
		BVHBuilder            builder;
		std::vector<FlatNode> nodes;
		std::vector<uint32_t> triIndices;
		builder.Build(localTriangles, nodes, triIndices);
		const auto bvhEnd = std::chrono::steady_clock::now();

		registered.nodes      = std::move(nodes);
		registered.triIndices = std::move(triIndices);
		registered.triangles  = std::move(localTriangles);
		registered.ownerGuid  = ownerGuid;
		registered.mobility   = mobility;
		registered.world      =
			mobility == ColliderMobility::Dynamic ? world : Mat4::identity;

		const auto  end           = std::chrono::steady_clock::now();
		const char* mobilityLabel = mobility == ColliderMobility::Dynamic ?
			                            "Dynamic" :
			                            "Static";
		Msg(
			"Physics",
			"Register{}Mesh timing: buildTris={}ms bvhBuild={}ms total={}ms (ownerGuid={} verts={} idx={} tris={})",
			mobilityLabel,
			std::chrono::duration_cast<std::chrono::milliseconds>(
				buildTrisEnd - buildTrisStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				bvhEnd - bvhStart
			).count(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				end - start
			).count(),
			ownerGuid,
			vertices.size(),
			indices.size(),
			registered.triangles.size()
		);

		return registered;
	}

	int Engine::BoxOverlapSet(
		const Box&                        box,
		Hit*                              outHits,
		const int                         maxHits,
		const std::vector<RegisteredBVH>& bvhSet
	) {
		if (maxHits <= 0 || outHits == nullptr || bvhSet.empty()) {
			return 0;
		}

		AABB boxAABB;
		boxAABB.min = box.center - box.halfSize;
		boxAABB.max = box.center + box.halfSize;

		std::vector<const RegisteredBVH*> filtered;
		filtered.reserve(bvhSet.size());

		for (const auto& bvh : bvhSet) {
			if (bvh.nodes.empty() || bvh.triangles.empty()) {
				continue;
			}
			const AABB rootBounds = ToWorldBounds(bvh, bvh.nodes[0].bounds);
			if (IsAABBOverlap(boxAABB, rootBounds)) {
				filtered.emplace_back(&bvh);
			}
		}
		if (filtered.size() > 1) {
			std::ranges::sort(
				filtered,
				[](const RegisteredBVH* lhs, const RegisteredBVH* rhs) {
					return lhs->ownerGuid < rhs->ownerGuid;
				}
			);
		}

		struct EntityOverlapAggregate {
			uint64_t guid       = 0;
			Hit      deepestHit = {};
			bool     hasDeepest = false;
			Hit      groundHit  = {};
			bool     hasGround  = false;
		};
		// 1エンティティがヒット配列を埋め尽くすと他メッシュの重なり情報が欠落するため、
		// 一度エンティティ単位で集約してから代表ヒットを生成します。
		std::vector<EntityOverlapAggregate> aggregates = {};
		aggregates.reserve(filtered.size());
		const auto FindAggregate = [&aggregates](const uint64_t guid) -> int {
			for (int i = 0; i < static_cast<int>(aggregates.size()); ++i) {
				if (aggregates[i].guid == guid) {
					return i;
				}
			}
			return -1;
		};

		uint32_t stack[64];
		for (auto* bvh : filtered) {
			int sp      = 0;
			stack[sp++] = 0;

			while (sp) {
				const uint32_t index      = stack[--sp];
				const auto&    node       = bvh->nodes[index];
				const AABB     nodeBounds = ToWorldBounds(*bvh, node.bounds);

				if (!IsAABBOverlap(boxAABB, nodeBounds)) {
					continue;
				}

				if (node.primCount == 0) {
					stack[sp++] = node.leftFirst;
					stack[sp++] = node.rightFirst;
				} else {
					const uint32_t first = node.leftFirst;
					for (uint32_t i = 0; i < node.primCount; ++i) {
						const uint32_t triIdx = bvh->triIndices[first + i];
						const Triangle tri    = ToWorldTriangle(*bvh, triIdx);

						Vec3  separationAxis;
						float penetrationDepth;
						if (!BoxVsTriangleOverlap(
							box,
							tri,
							separationAxis,
							penetrationDepth
						)) {
							continue;
						}

						if (separationAxis.SqrLength() > 1.0e-12f) {
							separationAxis.Normalize();
						}

						Hit tmpHit;
						tmpHit.toi   = 1.0f;
						tmpHit.depth = penetrationDepth;
						tmpHit.pos   = box.center + separationAxis * (
							             std::min(
								             {
									             box.halfSize.x,
									             box.halfSize.y,
									             box.halfSize.z
								             }
							             ) - penetrationDepth * 0.5f);
						tmpHit.normal        = separationAxis;
						tmpHit.triIndex      = triIdx;
						tmpHit.hitEntityGuid = bvh->ownerGuid;

						int aggregateIndex = FindAggregate(
							tmpHit.hitEntityGuid
						);
						if (aggregateIndex < 0) {
							aggregates.emplace_back();
							aggregateIndex =
								static_cast<int>(aggregates.size()) -
								1;
							aggregates[aggregateIndex].guid = tmpHit.
								hitEntityGuid;
						}

						EntityOverlapAggregate& aggregate =
							aggregates[aggregateIndex];
						if (
							!aggregate.hasDeepest ||
							tmpHit.depth > aggregate.deepestHit.depth ||
							(std::abs(
								 tmpHit.depth - aggregate.deepestHit.depth
							 ) <=
							 1.0e-6f &&
							 tmpHit.triIndex < aggregate.deepestHit.triIndex)
						) {
							aggregate.deepestHit = tmpHit;
							aggregate.hasDeepest = true;
						}

						const bool betterGroundNormal =
							!aggregate.hasGround ||
							tmpHit.normal.y > aggregate.groundHit.normal.y +
							1.0e-6f;
						const bool betterGroundDepth =
							aggregate.hasGround &&
							std::abs(
								tmpHit.normal.y - aggregate.groundHit.normal.y
							) <= 1.0e-6f &&
							tmpHit.depth > aggregate.groundHit.depth;
						if (betterGroundNormal || betterGroundDepth) {
							aggregate.groundHit = tmpHit;
							aggregate.hasGround = true;
						}
					}
				}
			}
		}

		if (aggregates.empty()) {
			return 0;
		}

		std::ranges::sort(
			aggregates,
			[](
			const EntityOverlapAggregate& lhs, const EntityOverlapAggregate& rhs
		) {
				return lhs.guid < rhs.guid;
			}
		);

		std::vector<Hit> emittedHits = {};
		emittedHits.reserve(aggregates.size() * 2);
		for (const EntityOverlapAggregate& aggregate : aggregates) {
			if (!aggregate.hasDeepest) {
				continue;
			}
			emittedHits.emplace_back(aggregate.deepestHit);
			if (
				aggregate.hasGround &&
				aggregate.groundHit.triIndex != aggregate.deepestHit.triIndex
			) {
				emittedHits.emplace_back(aggregate.groundHit);
			}
		}
		if (emittedHits.empty()) {
			return 0;
		}

		if (emittedHits.size() > 1) {
			std::ranges::sort(
				emittedHits,
				[](const Hit& lhs, const Hit& rhs) {
					if (lhs.hitEntityGuid != rhs.hitEntityGuid) {
						return lhs.hitEntityGuid < rhs.hitEntityGuid;
					}
					if (lhs.depth != rhs.depth) {
						return lhs.depth > rhs.depth;
					}
					if (lhs.triIndex != rhs.triIndex) {
						return lhs.triIndex < rhs.triIndex;
					}
					if (lhs.normal.x != rhs.normal.x) {
						return lhs.normal.x < rhs.normal.x;
					}
					if (lhs.normal.y != rhs.normal.y) {
						return lhs.normal.y < rhs.normal.y;
					}
					if (lhs.normal.z != rhs.normal.z) {
						return lhs.normal.z < rhs.normal.z;
					}
					return false;
				}
			);
		}

		const int outCount = std::min(
			maxHits,
			static_cast<int>(emittedHits.size())
		);
		std::copy_n(emittedHits.data(), outCount, outHits);
		return outCount;
	}

	int Engine::BoxOverlapSetRaw(
		const Box&                        box,
		Hit*                              outHits,
		const int                         maxHits,
		const std::vector<RegisteredBVH>& bvhSet
	) {
		if (maxHits <= 0 || outHits == nullptr || bvhSet.empty()) {
			return 0;
		}

		AABB boxAABB;
		boxAABB.min = box.center - box.halfSize;
		boxAABB.max = box.center + box.halfSize;

		std::vector<const RegisteredBVH*> filtered;
		filtered.reserve(bvhSet.size());

		for (const auto& bvh : bvhSet) {
			if (bvh.nodes.empty() || bvh.triangles.empty()) {
				continue;
			}
			const AABB rootBounds = ToWorldBounds(bvh, bvh.nodes[0].bounds);
			if (IsAABBOverlap(boxAABB, rootBounds)) {
				filtered.emplace_back(&bvh);
			}
		}
		if (filtered.size() > 1) {
			std::ranges::sort(
				filtered,
				[](const RegisteredBVH* lhs, const RegisteredBVH* rhs) {
					return lhs->ownerGuid < rhs->ownerGuid;
				}
			);
		}

		int      outCount = 0;
		uint32_t stack[64];
		for (auto* bvh : filtered) {
			int sp      = 0;
			stack[sp++] = 0;

			while (sp) {
				const uint32_t index = stack[--sp];
				const auto& [bounds, leftFirst, rightFirst, primCount] = bvh->
					nodes[index];
				const AABB nodeBounds = ToWorldBounds(*bvh, bounds);

				if (!IsAABBOverlap(boxAABB, nodeBounds)) {
					continue;
				}

				if (primCount == 0) {
					stack[sp++] = leftFirst;
					stack[sp++] = rightFirst;
				} else {
					const uint32_t first = leftFirst;
					for (uint32_t i = 0; i < primCount; ++i) {
						if (outCount >= maxHits) {
							return outCount;
						}

						const uint32_t triIdx = bvh->triIndices[first + i];
						const Triangle tri    = ToWorldTriangle(*bvh, triIdx);

						Vec3  separationAxis;
						float penetrationDepth;
						if (!BoxVsTriangleOverlap(
							box,
							tri,
							separationAxis,
							penetrationDepth
						)) {
							continue;
						}

						if (separationAxis.SqrLength() > 1.0e-12f) {
							separationAxis.Normalize();
						}

						Hit& out  = outHits[outCount++];
						out.toi   = 1.0f;
						out.depth = penetrationDepth;
						out.pos   = box.center + separationAxis * (
							          std::min(
								          {
									          box.halfSize.x,
									          box.halfSize.y,
									          box.halfSize.z
								          }
							          ) - penetrationDepth * 0.5f);
						out.normal        = separationAxis;
						out.triIndex      = triIdx;
						out.hitEntityGuid = bvh->ownerGuid;
						out.startSolid    = true;
						out.allsolid      = false;
					}
				}
			}
		}

		return outCount;
	}
}
