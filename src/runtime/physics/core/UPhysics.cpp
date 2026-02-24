#include "UPhysics.h"

#include <pch.h>
#include <vector>

#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/Debug/DebugDraw.h>
#include <engine/Entity/Entity.h>
#include <engine/ResourceSystem/Mesh/StaticMesh.h>
#include <engine/unnamed/subsystem/console/Log.h>
#include <engine/unnamed/uphysics/BoxCast.h>
#include <engine/unnamed/uphysics/PhysicsTypes.h>
#include <engine/unnamed/uphysics/RayCast.h>
#include <engine/unnamed/uphysics/SphereCast.h>

namespace UPhysics {
	/// @brief 初期化
	void Engine::Init() {
		// なんかする
	}

	/// @brief 更新
	void Engine::Update(float) const {
#ifdef _DEBUG
		const auto camera = CameraManager::GetActiveCamera();
		if (camera) {
			Mat4 invView = camera->GetViewMat().Inverse();
			Vec3 start   = invView.GetTranslate();
			Vec3 dir     = invView.GetForward();

			dir.Normalize();
			const Unnamed::Ray ray = {
				.origin = start,
				.dir    = dir,
				.invDir = 1.0f / dir,
				.tMin   = 0.0f,
				.tMax   = 1e30f
			};

			DebugDraw::DrawAxis(
				start,
				Quaternion::identity
			);

			Hit hit;
			if (RayCast(ray, &hit)) {
				DebugDraw::DrawRay(
					start,
					dir * hit.t,
					Vec4::blue
				);
				DebugDraw::DrawAxis(
					hit.pos,
					Quaternion::identity
				);
				DebugDraw::DrawRay(
					hit.pos,
					hit.normal,
					Vec4::magenta
				);
			}
		}
#endif
	}

	/// @brief エンティティを登録する関数
	/// @details メッシュコライダーを持ったエンティティを登録します
	/// @param entity 登録するエンティティ(旧)
	void Engine::RegisterEntity(Entity* entity) {
		if (!entity) { return; }
		
		const bool alreadyRegistered = std::ranges::any_of(
			mBVHs,
			[&](const RegisteredBVH& bvh) { return bvh.owner == entity; }
		);
		if (alreadyRegistered) {
			UnregisterEntity(entity);
		}

		auto meshCollider = entity->GetComponent<MeshColliderComponent>();
		if (!meshCollider) {
			Warning(
				"UPhysics",
				"Entity '{}' MeshColliderComponent is null.",
				entity->GetName()
			);
			return;
		}

		const auto  transform = meshCollider->GetOwner()->GetTransform();
		const Mat4& worldMat  = transform->GetWorldMat();

		const float m00 = worldMat.m[0][0], m01 = worldMat.m[0][1], m02 =
			            worldMat.m[0][2];
		const float m10 = worldMat.m[1][0], m11 = worldMat.m[1][1], m12 =
			            worldMat.m[1][2];
		const float m20 = worldMat.m[2][0], m21 = worldMat.m[2][1], m22 =
			            worldMat.m[2][2];
		const float m30 = worldMat.m[3][0], m31 = worldMat.m[3][1], m32 =
			            worldMat.m[3][2];

		for (
			const auto& subMesh : meshCollider->GetStaticMesh()->GetSubMeshes()
		) {
			const auto& tris = subMesh->GetPolygons();

			std::vector<Unnamed::Triangle> triangles;
			triangles.reserve(tris.size());

			// UPhysics::Triangleに変換
			for (const auto& tri : tris) {
				triangles.emplace_back(
					Vec3(
						tri.v0.x * m00 + tri.v0.y * m10 + tri.v0.z * m20 + m30,
						tri.v0.x * m01 + tri.v0.y * m11 + tri.v0.z * m21 + m31,
						tri.v0.x * m02 + tri.v0.y * m12 + tri.v0.z * m22 + m32
					),
					Vec3(
						tri.v1.x * m00 + tri.v1.y * m10 + tri.v1.z * m20 + m30,
						tri.v1.x * m01 + tri.v1.y * m11 + tri.v1.z * m21 + m31,
						tri.v1.x * m02 + tri.v1.y * m12 + tri.v1.z * m22 + m32
					),
					Vec3(
						tri.v2.x * m00 + tri.v2.y * m10 + tri.v2.z * m20 + m30,
						tri.v2.x * m01 + tri.v2.y * m11 + tri.v2.z * m21 + m31,
						tri.v2.x * m02 + tri.v2.y * m12 + tri.v2.z * m22 + m32
					)
				);
			}

			// BVHを構築
			BVHBuilder            bvhBuilder;
			std::vector<FlatNode> nodes;
			std::vector<uint32_t> triIndices;

			size_t triStart = mTriangles.size();

			bvhBuilder.Build(triangles, nodes, triIndices);

			size_t triCount = triangles.size();

			AddGlobalOffset(
				triIndices,
				static_cast<uint32_t>(mTriangles.size())
			);

			mBVHs.emplace_back(
				RegisteredBVH{
					.nodes      = std::move(nodes),
					.triIndices = std::move(triIndices),
					.triStart   = triStart,
					.triCount   = triCount,
					.owner      = entity
				}
			);

			mTriangles.insert(
				mTriangles.end(),
				triangles.begin(),
				triangles.end()
			);
		}
	}

	/// @brief エンティティの登録を解除する関数
	/// @param entity 登録解除するエンティティ
	void Engine::UnregisterEntity(const Entity* entity) {
		if (mBVHs.empty()) { return; }

		struct DelRange {
			size_t start, count;
		};
		std::vector<DelRange> ranges;

		std::erase_if(
			mBVHs,
			[&](const RegisteredBVH& bvh) {
				if (bvh.owner != entity) { return false; }
				ranges.emplace_back(bvh.triStart, bvh.triCount);
				return true;
			}
		);

		if (ranges.empty()) { return; }

		std::ranges::sort(
			ranges, [](auto& a, auto& b) { return a.start > b.start; }
		);

		for (auto& [start, count] : ranges) {
			mTriangles.erase(
				mTriangles.begin() + static_cast<long long>(start),
				mTriangles.begin() + static_cast<long long>(start + count)
			);

			for (auto& bvh : mBVHs) {
				if (bvh.triStart > start) {
					bvh.triStart -= count;
					for (uint32_t& id : bvh.triIndices) {
						if (id >= start) { id -= static_cast<uint32_t>(count); }
					}
				}
			}
		}
	}

	/// @brief レイキャストを行う関数
	/// @param ray レイ情報
	/// @param outHit 衝突情報の出力先
	/// @return 衝突した場合trueを返す
	bool Engine::RayCast(const Unnamed::Ray& ray, Hit* outHit) const {
		UPhysics::RayCast cast;
		cast.start  = ray.origin;
		cast.invDir = ray.invDir;
		return CastBVH(
			cast, ray.origin, ray.dir, ray.tMax, outHit, mBVHs,
			mTriangles
		);
	}

	/// @brief ボックスキャストを行う関数
	/// @param box ボックス情報
	/// @param dir キャスト方向
	/// @param length キャスト距離
	/// @param outHit 衝突情報の出力先
	/// @return 衝突した場合trueを返す
	bool Engine::BoxCast(
		const Unnamed::Box& box,
		const Vec3&         dir,
		const float         length,
		Hit*                outHit
	) const {
		Vec3  dirN = dir;
		float len  = length;

		float dirLen = dirN.Length();
		if (dirLen > 1e-6f) {
			dirN /= dirLen;
			if (fabs(len - dirLen) < 1e-4f) len = dirLen;
		} else {
			return false; // ゼロ方向なら衝突無し
		}

		UPhysics::BoxCast caster;
		caster.box  = box;
		caster.half = box.halfSize;

		return CastBVH(
			caster,
			box.center,
			dirN,
			len,
			outHit,
			mBVHs, mTriangles
		);
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
		float       radius,
		const Vec3& dir,
		const float length,
		Hit*        outHit
	) const {
		UPhysics::SphereCast cast;
		cast.center = start;
		cast.radius = radius;

		return CastBVH(cast, start, dir, length, outHit, mBVHs, mTriangles);
	}

	/// @brief ボックスとメッシュの重なり判定を行う関数
	/// @param box ボックス情報
	/// @param outHit 衝突情報の出力先
	/// @return 重なりがあった場合trueを返す
	bool Engine::BoxOverlap(
		const Unnamed::Box& box,
		Hit*                outHit
	) const {
		if (mBVHs.empty() || mTriangles.empty()) { return false; }

		// ブロードフェーズ：ボックスのAABBと各BVHのルートAABBの重なりをチェック
		std::vector<const RegisteredBVH*> filtered;
		Unnamed::AABB                     boxAABB;
		boxAABB.min = box.center - box.halfSize;
		boxAABB.max = box.center + box.halfSize;

		for (const auto& bvh : mBVHs) {
			const Unnamed::AABB& rootBounds = bvh.nodes[0].bounds;
			// AABB同士の重なり判定
			if (boxAABB.max.x >= rootBounds.min.x && boxAABB.min.x <= rootBounds
			    .max.x &&
			    boxAABB.max.y >= rootBounds.min.y && boxAABB.min.y <= rootBounds
			    .max.y &&
			    boxAABB.max.z >= rootBounds.min.z && boxAABB.min.z <= rootBounds
			    .max.z) { filtered.emplace_back(&bvh); }
		}

		if (filtered.empty()) { return false; }

		// ナローフェーズ：詳細な重なり判定
		float    minPenetration = FLT_MAX;
		uint32_t hitTri         = UINT32_MAX;
		Vec3     hitNormal;
		Vec3     hitPos;
		uint32_t stack[64];
		Entity*  hitEntity = nullptr;

		for (auto* bvh : filtered) {
			int sp      = 0;
			stack[sp++] = 0; // ルートノードからスタート

			while (sp) {
				const uint32_t index = stack[--sp];
				const auto&    node  = bvh->nodes[index];

				// ノードのAABBとボックスの重なり判定
				if (!(boxAABB.max.x >= node.bounds.min.x && boxAABB.min.x <=
				      node.bounds.max.x &&
				      boxAABB.max.y >= node.bounds.min.y && boxAABB.min.y <=
				      node.
				      bounds.max.y &&
				      boxAABB.max.z >= node.bounds.min.z && boxAABB.min.z <=
				      node.
				      bounds.max.z)) {
					continue; // 重なりなし
				}

				if (node.primCount == 0) {
					// 内部ノード：子ノードをスタックに追加
					stack[sp++] = node.leftFirst;
					stack[sp++] = node.rightFirst;
				} else {
					// 葉ノード：三角形との詳細判定
					uint32_t first = node.leftFirst;
					for (uint32_t i = 0; i < node.primCount; ++i) {
						uint32_t triIdx = bvh->triIndices[first + i];
						const Unnamed::Triangle& tri = mTriangles[triIdx];

						Vec3  separationAxis;
						float penetrationDepth;
						if (BoxVsTriangleOverlap(
							box, tri, separationAxis, penetrationDepth
						)) {
							if (penetrationDepth < minPenetration) {
								minPenetration = penetrationDepth;
								hitTri = triIdx;
								hitNormal = separationAxis; // already outward
								hitPos = box.center + hitNormal * (std::min(
									         {
										         box.halfSize.x,
										         box.halfSize.y,
										         box.halfSize.z
									         }
								         ) - penetrationDepth * 0.5f);
								hitEntity = bvh->owner;
							}
						}
					}
				}
			}
		}

		if (hitTri == UINT32_MAX) {
			return false; // 重なりなし
		}

		if (outHit) {
			outHit->t         = 1.0f;           // sweep 用でないので 1
			outHit->depth     = minPenetration; // ← depth をセット
			outHit->pos       = hitPos;
			outHit->normal    = hitNormal;
			outHit->triIndex  = hitTri;
			outHit->hitEntity = hitEntity;
		}
		return true;
	}

	/// @brief ボックスとメッシュの重なり判定を行う関数（複数ヒット版）
	/// @param box ボックス情報
	/// @param outHits 衝突情報の出力先配列
	/// @param maxHits 出力先配列の最大ヒット数
	/// @return ヒットした数を返す
	int Engine::BoxOverlap(
		const Unnamed::Box& box,
		Hit*                outHits,
		int                 maxHits
	) const {
		int hitCount = 0;
		if (mBVHs.empty() || mTriangles.empty() || maxHits <= 0) {
			return hitCount;
		}

		// ブロードフェーズ：ボックスのAABBと各BVHのルートAABBの重なりをチェック
		std::vector<const RegisteredBVH*> filtered;
		Unnamed::AABB                     boxAABB;
		boxAABB.min = box.center - box.halfSize;
		boxAABB.max = box.center + box.halfSize;

		for (const auto& bvh : mBVHs) {
			const Unnamed::AABB& rootBounds = bvh.nodes[0].bounds;
			// AABB同士の重なり判定
			if (boxAABB.max.x >= rootBounds.min.x && boxAABB.min.x <= rootBounds
			    .max.x &&
			    boxAABB.max.y >= rootBounds.min.y && boxAABB.min.y <= rootBounds
			    .max.y &&
			    boxAABB.max.z >= rootBounds.min.z && boxAABB.min.z <= rootBounds
			    .max.z) { filtered.emplace_back(&bvh); }
		}

		if (filtered.empty()) {
			return hitCount; // 重なりなし
		}

		// ナローフェーズ：詳細な重なり判定
		uint32_t stack[64];

		for (auto* bvh : filtered) {
			int sp      = 0;
			stack[sp++] = 0; // ルートノードからスタート

			while (sp && hitCount < maxHits) {
				const uint32_t index = stack[--sp];
				const auto&    node  = bvh->nodes[index];

				// ノードのAABBとボックスの重なり判定
				if (!(boxAABB.max.x >= node.bounds.min.x && boxAABB.min.x <=
				      node.bounds.max.x &&
				      boxAABB.max.y >= node.bounds.min.y && boxAABB.min.y <=
				      node.
				      bounds.max.y &&
				      boxAABB.max.z >= node.bounds.min.z && boxAABB.min.z <=
				      node.
				      bounds.max.z)) {
					continue; // 重なりなし
				}

				if (node.primCount == 0) {
					// 内部ノード：子ノードをスタックに追加
					stack[sp++] = node.leftFirst;
					stack[sp++] = node.rightFirst;
				} else {
					// 葉ノード：三角形との詳細判定
					uint32_t first = node.leftFirst;
					for (uint32_t i = 0; i < node.primCount && hitCount <
					                     maxHits; ++i) {
						uint32_t triIdx = bvh->triIndices[first + i];
						const Unnamed::Triangle& tri = mTriangles[triIdx];

						Vec3  separationAxis;
						float penetrationDepth;
						if (BoxVsTriangleOverlap(
							box, tri, separationAxis, penetrationDepth
						)) {
							Hit tmpHit;
							tmpHit.t     = 1.0f;
							tmpHit.depth = penetrationDepth;
							tmpHit.pos   = box.center + separationAxis * (
								               std::min(
									               {
										               box.halfSize.x,
										               box.halfSize.y,
										               box.halfSize.z
									               }
								               ) - penetrationDepth * 0.5f);
							tmpHit.normal    = separationAxis;
							tmpHit.triIndex  = triIdx;
							tmpHit.hitEntity = bvh->owner;

							outHits[hitCount] = tmpHit;
							hitCount++;
						}
					}
				}
			}
		}

		return hitCount;
	}

	/// @brief インデックスにグローバルオフセットを追加します
	/// @param indices インデックス配列
	/// @param base オフセット値
	void Engine::AddGlobalOffset(
		std::vector<uint32_t>& indices,
		const uint32_t         base
	) { for (uint32_t& index : indices) { index += base; } }
}
