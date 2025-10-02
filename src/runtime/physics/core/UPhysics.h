#pragma once
#include <engine/Debug/Debug.h>
#include <engine/uphysics/BVH.h>
#include <engine/uphysics/BVHBuilder.h>
#include <engine/uphysics/CollisionDetection.h>

namespace UPhysics {
	// 物理エンジン
	class Engine {
	public:
		void Init();
		void Update(float deltaTime) const;

		void RegisterEntity(Entity* entity);
		void UnregisterEntity(const Entity* entity);

		bool RayCast(
			const Unnamed::Ray& ray,
			Hit* outHit
		) const;

		bool BoxCast(
			const Unnamed::Box& box,
			const Vec3& dir,
			float       length, Hit* outHit
		) const;

		bool SphereCast(
			const Vec3& start,
			float       radius,
			const Vec3& dir,
			float       length,
			Hit* outHit
		) const;

		bool BoxOverlap(
			const Unnamed::Box& box,
			Hit* outHit
		) const;

		int BoxOverlap(
			const Unnamed::Box& box,
			Hit* outHit,
			int        maxHits
		) const;

	private:
		template <class CastType>
		static bool CastBVH(
			const CastType& cast,
			const Vec3& start,
			const Vec3& dir,
			float                             length,
			Hit* outHit,
			const std::vector<RegisteredBVH>& bvhSet,
			const std::vector<Unnamed::Triangle>& allTriangles
		) {
			// まずは各BVHのルートのAABBとレイが交差するかを確認
			// してなきゃ意味ないからね! これが噂のブロードフェーズ!
			std::vector<const RegisteredBVH*> filtered;
			const Unnamed::Ray                         broadRay = {
				.origin = start,
				.dir = dir,
				.invDir = Vec3::one / dir,
				.tMin = 0.0f,
				.tMax = length
			};

			for (const auto& bvh : bvhSet) {
				Unnamed::AABB  root = cast.ExpandNode(bvh.nodes[0].bounds);
				float t = length;
				if (RayVsAABB(broadRay, root, t)) {
					filtered.emplace_back(&bvh);
				}
			}

			if (filtered.empty()) {
				// 交差しなかった...
				return false;
			}

			// 本格的に探索する
			// 一番近い衝突のTOI (TOI: Time of Impact 衝突までの時間[0.0f ～ 1.0f])
			float    bestTOI = 1.0f;
			uint32_t hitTri = UINT32_MAX; // ヒットした三角形のインデックス
			Vec3     hitNormal;            // ヒットした法線
			uint32_t stack[64];            // スタックを使ってBVHを探索(深さ優先探索)

			// ブロードフェーズで検知されたBVHを探索する
			for (auto* bvh : filtered) {
				int sp = 0;
				stack[sp++] = 0; // ルートノードからスタート

				while (sp) {
					const uint32_t index = stack[--sp];
					const auto& node = bvh->nodes[index];

#ifdef _DEBUG
					Vec3 center = (node.bounds.min + node.bounds.max) *
						0.5f;
					const Vec3 size = node.bounds.max - node.bounds.min;
					Debug::DrawBox(
						center,
						Quaternion::identity,
						size,
						Vec4::orange
					);
#endif

					// 現在の最良TOIを使った早期終了
					Unnamed::Ray pruneRay = broadRay;
					pruneRay.tMax = bestTOI * length;
					float tBox = bestTOI * length;
					if (
						!RayVsAABB(
							pruneRay,
							cast.ExpandNode(node.bounds),
							tBox
						)
						) {
						continue; // 残念!
					}

					if (node.primCount == 0) {
						// 近い子ノードを探索する
						stack[sp++] = node.leftFirst;
						stack[sp++] = node.rightFirst;
					}
					else {
						uint32_t first = node.leftFirst;
						for (uint32_t i = 0; i < node.primCount; ++i) {
							uint32_t triIdx = bvh->triIndices[first + i];
							float    toi;
							Vec3     nrm;
							if (cast.TestTriangle(
								allTriangles[triIdx], dir, length,
								toi, nrm)) {
								if (toi < bestTOI) {
									bestTOI = toi;
									hitTri = triIdx;
									hitNormal = nrm;
									// TOIが更新されたら、より厳しい早期終了を設定
									pruneRay.tMax = bestTOI * length;
								}
							}
						}
					}
				}
			}

			if (hitTri == UINT32_MAX) {
				return false; // 残念!
			}
			if (outHit) {
				outHit->t = bestTOI * length;
				outHit->pos = start + dir * outHit->t;
				outHit->normal = hitNormal;
				outHit->triIndex = hitTri;
			}
			return true;
		}

		static void AddGlobalOffset(std::vector<uint32_t>& indices,
			uint32_t               base);

		std::vector<Unnamed::Triangle> mTriangles;
		std::vector<FlatNode> mNodes;
		std::vector<uint32_t> mTriIndices;

		std::vector<RegisteredBVH> mBVHs;
	};
}
