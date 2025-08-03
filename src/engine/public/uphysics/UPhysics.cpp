#include <pch.h>
#include <vector>

#include <engine/public/Camera/CameraManager.h>
#include <engine/public/Components/Camera/CameraComponent.h>
#include <engine/public/Components/ColliderComponent/MeshColliderComponent.h>
#include <engine/public/Debug/Debug.h>
#include <engine/public/ResourceSystem/Mesh/StaticMesh.h>
#include <engine/public/subsystem/console/Log.h>
#include <engine/public/uphysics/BoxCast.h>
#include <engine/public/uphysics/Primitives.h>
#include <engine/public/uphysics/RayCast.h>
#include <engine/public/uphysics/UPhysics.h>

namespace UPhysics {
	void Engine::Init() {
		// なんかする
	}

	void Engine::Update(float) {
		const auto camera = CameraManager::GetActiveCamera();
		if (camera) {
			Mat4 invView = camera->GetViewMat().Inverse();
			Vec3 start   = invView.GetTranslate();
			Vec3 dir     = invView.GetForward();

			dir.Normalize();
			Ray ray = {
				start,
				dir,
				1.0f / dir,
				0.0f,
				1e30f
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

			Box box    = {};
			box.center = start;
			box.half   = Vec3::one * 0.5f;
			if (BoxCast(box, dir, 65536.0f, &hit)) {
				ImGui::Begin("Hit");
				ImGui::End();
				Debug::DrawBox(
					hit.pos,
					Quaternion::identity,
					box.half * 2.0f,
					Vec4::blue
				);
				Debug::DrawBox(
					box.center + dir * hit.t,
					Quaternion::identity,
					box.half * 2.0f,
					Vec4::green
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
		}
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


			std::vector<Triangle> triangles;

			// UPhysics::Triangleに変換 TODO: すべてのTrinagleをUPhysics::Triangleに変更する
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
					std::move(nodes),
					std::move(triIndices),
					triStart,
					triCount,
					entity
				}
			);
			mTriangles.insert(
				mTriangles.end(),
				triangles.begin(),
				triangles.end()
			);
		}
	}

	void Engine::UnregisterEntity(Entity* entity) {
		if (mBVHs.empty()) {
			return;
		}

		struct DelRange {
			size_t start, count;
		};
		std::vector<DelRange> ranges;

		mBVHs.erase(
			std::remove_if(
				mBVHs.begin(), mBVHs.end(),
				[&](const RegisteredBVH& bvh) {
					if (bvh.owner != entity) {
						return false;
					}
					ranges.emplace_back(bvh.triStart, bvh.triCount);
					return true;
				}
			),
			mBVHs.end()
		);

		if (ranges.empty()) {
			return;
		}

		std::sort(ranges.begin(), ranges.end(), [](auto& a, auto& b) {
			return a.start > b.start;
		});

		for (auto& r : ranges) {
			mTriangles.erase(
				mTriangles.begin() + r.start,
				mTriangles.begin() + r.start + r.count
			);

			for (auto& bvh : mBVHs) {
				if (bvh.triStart > r.start) {
					bvh.triStart -= r.count;
					for (uint32_t& id : bvh.triIndices) {
						if (id >= r.start) {
							id -= static_cast<uint32_t>(r.count);
						}
					}
				}
			}
		}
	}

	bool Engine::RayCast(const Ray& ray, Hit* outHit) const {
		UPhysics::RayCast cast;
		cast.start  = ray.start;
		cast.invDir = ray.invDir;
		return CastBVH(cast, ray.start, ray.dir, ray.tMax, outHit, mBVHs,
		               mTriangles);
	}

	bool Engine::BoxCast(
		const Box&  box,
		const Vec3& dir,
		float       length,
		Hit*        outHit
	) {
		/* 1) 方向を正規化し、実距離を分離 */
		Vec3  dirN = dir;
		float len  = length;

		float dirLen = dirN.Length();
		if (dirLen > 1e-6f) {
			// 非ゼロなら正規化
			dirN /= dirLen;
			/* もし rawDir が「移動ベクトル」なら距離を上書き */
			if (fabs(len - dirLen) < 1e-4f) // 呼び出し側が同じ値を渡している場合
				len = dirLen;               // → len=|rawDir|
		} else {
			return false; // ゼロ方向なら衝突無し
		}

		/* 2) キャスターをセット */
		UPhysics::BoxCast caster;
		caster.box  = box;
		caster.half = box.half; // ← ExpandNode 用

		/* 3) CastBVH は dirN + len で呼ぶ */
		return CastBVH(caster,
		               box.center,
		               dirN, // ★ 正規化済み
		               len,  // ★ 実距離
		               outHit,
		               mBVHs, mTriangles);

		// if (mBVHs.empty()) {
		// 	return false;
		// }
		// Ray ray;
		// ray.start  = box.center;
		// ray.dir    = dir;
		// ray.invDir = Vec3(
		// 	dir.x != 0.0f ? 1.0f / dir.x : 1e30f,
		// 	dir.y != 0.0f ? 1.0f / dir.y : 1e30f,
		// 	dir.z != 0.0f ? 1.0f / dir.z : 1e30f
		// );
		// ray.tMin = 0.0f;
		// ray.tMax = length;
		//
		// float    closest = ray.tMax;
		// uint32_t hitTri  = UINT32_MAX;
		// Vec3     hitNormal;
		//
		// // とりあえずBVHのルートと交差するか見る
		// std::vector<const RegisteredBVH*> filtered;
		// for (const auto& bvh : mBVHs) {
		// 	AABB grown = bvh.nodes[0].bounds;
		// 	grown.min -= box.half;
		// 	grown.max += box.half;
		//
		// 	float temp = ray.tMax;
		// 	if (RayVsAABB(ray, grown, temp)) {
		// 		filtered.emplace_back(&bvh);
		// 	}
		// }
		//
		// float bestTOI = 1.0f; // 最も近い衝突のTOI
		//
		// uint32_t stack[64];
		//
		// for (const auto* bvh : filtered) {
		// 	int sp      = 0;
		// 	stack[sp++] = 0; // ルート
		//
		// 	while (sp) {
		// 		const uint32_t  index = stack[--sp];
		// 		const FlatNode& node  = bvh->nodes[index];
		//
		// 		// とりあえず使用されるBVHを描画しておく
		// 		Vec3       center = (node.bounds.min + node.bounds.max) * 0.5f;
		// 		const Vec3 size   = node.bounds.max - node.bounds.min;
		// 		Debug::DrawBox(
		// 			center,
		// 			Quaternion::identity,
		// 			size,
		// 			Vec4::orange
		// 		);
		//
		// 		AABB grown = node.bounds;
		// 		grown.min -= box.half;
		// 		grown.max += box.half;
		//
		// 		float tBox = closest;
		//
		// 		if (!RayVsAABB(ray, grown, tBox)) {
		// 			continue;
		// 		}
		//
		// 		if (node.primCount == 0) {
		// 			// 左の子のAABBを拡張
		// 			AABB grownLeft = bvh->nodes[node.leftFirst].bounds;
		// 			grownLeft.min -= box.half;
		// 			grownLeft.max += box.half;
		//
		// 			// 右の子のAABBを拡張
		// 			AABB grownRight = bvh->nodes[node.rightFirst].bounds;
		// 			grownRight.min -= box.half;
		// 			grownRight.max += box.half;
		//
		// 			float tBoxLeft = closest, tBoxRight = closest;
		// 			bool  hitLeft  = RayVsAABB(ray, grownLeft, tBoxLeft);
		// 			bool  hitRight = RayVsAABB(ray, grownRight, tBoxRight);
		//
		// 			if (hitLeft && hitRight) {
		// 				if (tBoxLeft < tBoxRight) {
		// 					stack[sp++] = node.rightFirst;
		// 					stack[sp++] = node.leftFirst;
		// 				} else {
		// 					stack[sp++] = node.leftFirst;
		// 					stack[sp++] = node.rightFirst;
		// 				}
		// 			} else if (hitLeft) {
		// 				stack[sp++] = node.leftFirst;
		// 			} else if (hitRight) {
		// 				stack[sp++] = node.rightFirst;
		// 			}
		// 		} else {
		// 			uint32_t first = node.leftFirst;
		// 			for (uint32_t i = 0; i < node.primCount; ++i) {
		// 				uint32_t triIndex = bvh->triIndices[first + i];
		// 				float    toi;
		// 				Vec3     normal;
		//
		// 				if (
		// 					SweptAabbVsTriSAT(
		// 						box, dir * length,
		// 						mTriangles[triIndex],
		// 						toi,
		// 						normal
		// 					)
		// 				) {
		// 					if (toi < bestTOI) {
		// 						bestTOI   = toi;
		// 						closest   = toi;
		// 						hitTri    = triIndex;
		// 						hitNormal = normal;
		// 					}
		// 				}
		// 			}
		// 		}
		// 	}
		// }
		//
		// if (hitTri == UINT32_MAX) {
		// 	return false;
		// }
		//
		// if (outHit) {
		// 	outHit->t        = bestTOI * length;
		// 	outHit->pos      = ray.start + ray.dir * outHit->t;
		// 	outHit->normal   = hitNormal;
		// 	outHit->triIndex = hitTri;
		// }
		//
		// return true;
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
