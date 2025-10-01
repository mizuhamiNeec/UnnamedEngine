#include "UPhysics.h"

#include <pch.h>
#include <vector>

#include <engine/Camera/CameraManager.h>
#include <engine/Components/Camera/CameraComponent.h>
#include <engine/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/Debug/Debug.h>
#include <engine/Entity/Entity.h>
#include <engine/ResourceSystem/Mesh/StaticMesh.h>
#include <engine/subsystem/console/Log.h>
#include <engine/uphysics/BoxCast.h>
#include <engine/uphysics/PhysicsTypes.h>
#include <engine/uphysics/RayCast.h>
#include <engine/uphysics/SphereCast.h>

namespace UPhysics {
	void Engine::Init() {
		// なんかする
	}

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
				.dir = dir,
				.invDir = 1.0f / dir,
				.tMin = 0.0f,
				.tMax = 1e30f
			};

			Debug::DrawAxis(
				start,
				Quaternion::identity
			);

			Hit hit;
			if (RayCast(ray, &hit)) {
				Debug::DrawRay(
					start,
					dir * hit.t,
					Vec4::blue
				);
				Debug::DrawAxis(
					hit.pos,
					Quaternion::identity
				);
				Debug::DrawRay(
					hit.pos,
					hit.normal,
					Vec4::magenta
				);
			}

			// const Box box = {
			// 	.center = start,
			// 	.half = Vec3::one * 0.5f
			// };
			// if (BoxCast(box, dir, 65536.0f, &hit)) {
			// 	ImGui::Begin("Hit");
			// 	ImGui::End();
			// 	Debug::DrawBox(
			// 		hit.pos,
			// 		Quaternion::identity,
			// 		box.half * 2.0f,
			// 		Vec4::blue
			// 	);
			// 	Debug::DrawBox(
			// 		box.center + dir * hit.t,
			// 		Quaternion::identity,
			// 		box.half * 2.0f,
			// 		Vec4::green
			// 	);
			// 	Debug::DrawAxis(
			// 		hit.pos,
			// 		Quaternion::identity
			// 	);
			// 	Debug::DrawRay(
			// 		hit.pos,
			// 		hit.normal,
			// 		Vec4::magenta
			// 	);
			// }
			//
			//
			// if (
			// 	SphereCast(
			// 		start,
			// 		0.5f,
			// 		dir,
			// 		65536.0f,
			// 		&hit
			// 	)
			// ) {
			// 	Debug::DrawSphere(
			// 		hit.pos,
			// 		Quaternion::identity,
			// 		0.5f,
			// 		Vec4::purple,
			// 		4
			// 	);
			// 	Debug::DrawAxis(
			// 		hit.pos,
			// 		Quaternion::identity
			// 	);
			// 	Debug::DrawRay(
			// 		hit.pos,
			// 		hit.normal,
			// 		Vec4::magenta
			// 	);
			// }
		}
#endif
	}

	/// @brief エンティティを登録する関数
	/// @details メッシュコライダーを持ったエンティティを登録します
	/// @param entity 登録するエンティティ
	void Engine::RegisterEntity(Entity* entity) {
		if (!entity->HasComponent<MeshColliderComponent>()) {
			Warning(
				"UPhysics",
				"Entity '{}' does not have a MeshColliderComponent.",
				entity->GetName()
			);
			return;
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

		const auto transform = meshCollider->GetOwner()->GetTransform();

		for (
			const auto& subMesh : meshCollider->GetStaticMesh()->GetSubMeshes()
		) {
			auto tris = subMesh->GetPolygons();


			std::vector<Unnamed::Triangle> triangles;

			// UPhysics::Triangleに変換 TODO: すべてのTriangleをUPhysics::Triangleに変更する
			for (auto tri : tris) {
				tri.v0 += transform->GetLocalPos();
				tri.v1 += transform->GetLocalPos();
				tri.v2 += transform->GetLocalPos();

				triangles.emplace_back(
					tri.v0, tri.v1, tri.v2
				);
			}

			// BVHを構築
			BVHBuilder            bvhBuilder;
			std::vector<FlatNode> nodes;
			std::vector<uint32_t> triIndices;

			size_t triStart = mTriangles.size();

			bvhBuilder.Build(triangles, nodes, triIndices);

			size_t triCount = triangles.size();

			// インデックスにグローバルオフセットを追加
			AddGlobalOffset(
				triIndices,
				static_cast<uint32_t>(mTriangles.size())
			);

			// メンバに突っ込む
			mBVHs.emplace_back(
				RegisteredBVH{
					.nodes = std::move(nodes),
					.triIndices = std::move(triIndices),
					.triStart = triStart,
					.triCount = triCount,
					.owner = entity
				}
			);
			mTriangles.insert(
				mTriangles.end(),
				triangles.begin(),
				triangles.end()
			);

			DevMsg(
				"UPhysics",
				"Registered entity '{}' with {} triangles.",
				subMesh->GetName(),
				subMesh->GetPolygons().size()
			);
		}
	}

	void Engine::UnregisterEntity(const Entity* entity) {
		if (mBVHs.empty()) {
			return;
		}

		struct DelRange {
			size_t start, count;
		};
		std::vector<DelRange> ranges;

		std::erase_if(
			mBVHs,
			[&](const RegisteredBVH& bvh) {
				if (bvh.owner != entity) {
					return false;
				}
				ranges.emplace_back(bvh.triStart, bvh.triCount);
				return true;
			}
		);

		if (ranges.empty()) {
			return;
		}

		std::ranges::sort(ranges, [](auto& a, auto& b) {
			return a.start > b.start;
		});

		for (auto& [start, count] : ranges) {
			mTriangles.erase(
				mTriangles.begin() + static_cast<long long>(start),
				mTriangles.begin() + static_cast<long long>(start + count)
			);

			for (auto& bvh : mBVHs) {
				if (bvh.triStart > start) {
					bvh.triStart -= count;
					for (uint32_t& id : bvh.triIndices) {
						if (id >= start) {
							id -= static_cast<uint32_t>(count);
						}
					}
				}
			}
		}
	}

	bool Engine::RayCast(const Unnamed::Ray& ray, Hit* outHit) const {
		UPhysics::RayCast cast;
		cast.start  = ray.origin;
		cast.invDir = ray.invDir;
		return CastBVH(cast, ray.origin, ray.dir, ray.tMax, outHit, mBVHs,
		               mTriangles);
	}

	bool Engine::BoxCast(
		const Unnamed::Box&  box,
		const Vec3& dir,
		const float length,
		Hit*        outHit
	) const {
		Vec3  dirN = dir;
		float len  = length;

		float dirLen = dirN.Length();
		if (dirLen > 1e-6f) {
			dirN /= dirLen;
			if (fabs(len - dirLen) < 1e-4f)
				len = dirLen;
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

	bool Engine::BoxOverlap(
		const Unnamed::Box& box,
		Hit*       outHit
	) const {
		if (mBVHs.empty() || mTriangles.empty()) {
			return false;
		}

		// ブロードフェーズ：ボックスのAABBと各BVHのルートAABBの重なりをチェック
		std::vector<const RegisteredBVH*> filtered;
		Unnamed::AABB                              boxAABB;
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
				.max.z) {
				filtered.emplace_back(&bvh);
			}
		}

		if (filtered.empty()) {
			return false;
		}

		// ナローフェーズ：詳細な重なり判定
		float    minPenetration = FLT_MAX;
		uint32_t hitTri         = UINT32_MAX;
		Vec3     hitNormal;
		Vec3     hitPos;
		uint32_t stack[64];

		for (auto* bvh : filtered) {
			int sp      = 0;
			stack[sp++] = 0; // ルートノードからスタート

			while (sp) {
				const uint32_t index = stack[--sp];
				const auto&    node  = bvh->nodes[index];

				// ノードのAABBとボックスの重なり判定
				if (!(boxAABB.max.x >= node.bounds.min.x && boxAABB.min.x <=
					node.bounds.max.x &&
					boxAABB.max.y >= node.bounds.min.y && boxAABB.min.y <= node.
					bounds.max.y &&
					boxAABB.max.z >= node.bounds.min.z && boxAABB.min.z <= node.
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
						uint32_t        triIdx = bvh->triIndices[first + i];
						const Unnamed::Triangle& tri    = mTriangles[triIdx];

						Vec3  separationAxis;
						float penetrationDepth;
						if (BoxVsTriangleOverlap(
							box, tri, separationAxis, penetrationDepth)) {
							if (penetrationDepth < minPenetration) {
								minPenetration = penetrationDepth;
								hitTri = triIdx;
								hitNormal = separationAxis; // already outward
								hitPos = box.center + hitNormal * (std::min(
									{
										box.halfSize.x, box.halfSize.y,
										box.halfSize.z
									}) - penetrationDepth * 0.5f);
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
			outHit->t        = 1.0f;           // sweep 用でないので 1
			outHit->depth    = minPenetration; // ← depth をセット
			outHit->pos      = hitPos;
			outHit->normal   = hitNormal;
			outHit->triIndex = hitTri;
		}
		return true;
	}

	int Engine::BoxOverlap(
		const Unnamed::Box& box,
		Hit*       outHits,
		int        maxHits
	) const {
		int hitCount = 0;
		if (mBVHs.empty() || mTriangles.empty() || maxHits <= 0) {
			return hitCount;
		}

		// ブロードフェーズ：ボックスのAABBと各BVHのルートAABBの重なりをチェック
		std::vector<const RegisteredBVH*> filtered;
		Unnamed::AABB                              boxAABB;
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
				.max.z) {
				filtered.emplace_back(&bvh);
			}
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
					boxAABB.max.y >= node.bounds.min.y && boxAABB.min.y <= node.
					bounds.max.y &&
					boxAABB.max.z >= node.bounds.min.z && boxAABB.min.z <= node.
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
						uint32_t        triIdx = bvh->triIndices[first + i];
						const Unnamed::Triangle& tri    = mTriangles[triIdx];

						Vec3  separationAxis;
						float penetrationDepth;
						if (BoxVsTriangleOverlap(
							box, tri, separationAxis, penetrationDepth)) {
							Hit tmpHit;
							tmpHit.t     = 1.0f;
							tmpHit.depth = penetrationDepth;
							tmpHit.pos   = box.center + separationAxis * (
								std::min({
									box.halfSize.x, box.halfSize.y, box.halfSize.z
								}) - penetrationDepth * 0.5f);
							tmpHit.normal   = separationAxis;
							tmpHit.triIndex = triIdx;

							outHits[hitCount] = tmpHit;
							hitCount++;
						}
					}
				}
			}
		}

		return hitCount;
	}

	void Engine::AddGlobalOffset(
		std::vector<uint32_t>& indices,
		const uint32_t         base
	) {
		for (uint32_t& index : indices) {
			index += base;
		}
	}
}
