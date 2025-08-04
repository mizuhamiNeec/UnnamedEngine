#include <engine/public/uphysics/CollisionDetection.h>

#include <algorithm>

#include <assimp/mesh.h>

namespace UPhysics {
	bool RayVsAABB(
		const Ray& ray, const AABB& aabb,
		float&     tMaxOut
	) {
		float tMin = ray.tMin;
		tMaxOut    = ray.tMax;

		for (int i = 0; i < 3; ++i) {
			if (fabs(ray.dir[i]) < 1e-8f) {
				if (ray.start[i] < aabb.min[i] || ray.start[i] > aabb.max[i])
					return false;
			} else {
				const float invD = 1.0f / ray.dir[i];
				float       t1   = (aabb.min[i] - ray.start[i]) * invD;
				float       t2   = (aabb.max[i] - ray.start[i]) * invD;
				if (t1 > t2) std::swap(t1, t2);
				tMin    = t1 > tMin ? t1 : tMin;
				tMaxOut = t2 < tMaxOut ? t2 : tMaxOut;
				if (tMin > tMaxOut)
					return false;
			}
		}
		return true;
	}

	bool TriangleVsRay(
		const Triangle& triangle, const Ray& ray,
		float&          tHit, Vec3&          outNormal
	) {
		constexpr float kEpsilon = 1e-6f;
		const Vec3      e1       = triangle.v1 - triangle.v0;
		const Vec3      e2       = triangle.v2 - triangle.v0;
		const Vec3      p        = ray.dir.Cross(e2);
		const float     det      = e1.Dot(p);

		if (fabs(det) < kEpsilon) {
			return false;
		}
		float invDet = 1.0f / det;

		Vec3  s = ray.start - triangle.v0;
		float u = s.Dot(p) * invDet;
		if (u < 0.0f || u > 1.0f) {
			return false;
		}

		Vec3  q = s.Cross(e1);
		float v = ray.dir.Dot(q) * invDet;
		if (v < 0.0f || u + v > 1.0f) {
			return false;
		}

		float t = e2.Dot(q) * invDet;
		if (t < ray.tMin || t > tHit) {
			return false; // レイは三角形と交差しない
		}

		tHit      = t;
		outNormal = e1.Cross(e2).Normalized();

		return true;
	}

	bool SweptAabbVsTriSAT(
		const Box&      box0,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNrm
	) {
		/* ---------- 1. 基本量 ---------- */
		const Vec3 C0 = box0.center;
		const Vec3 H  = box0.half;
		const Vec3 V  = delta; // 世界ベロシティ

		/* ---------- 2. 13 軸を列挙 ---------- */
		Vec3 axes[13];
		int  axisCount = 0;

		/* 2-1 三角形法線 */
		axes[axisCount++] =
			(tri.v1 - tri.v0).Cross(tri.v2 - tri.v0).Normalized();

		/* 2-2 ボックス軸 (世界 XYZ) */
		axes[axisCount++] = {1, 0, 0};
		axes[axisCount++] = {0, 1, 0};
		axes[axisCount++] = {0, 0, 1};

		/* 2-3 triEdge × boxAxis (交叉積) */
		Vec3 e0          = tri.v1 - tri.v0;
		Vec3 e1          = tri.v2 - tri.v1;
		Vec3 e2          = tri.v0 - tri.v2;
		Vec3 triEdges[3] = {e0, e1, e2};
		for (Vec3 e : triEdges) {
			if (e.Dot(e) < 1e-10f) continue; // degenerate
			Vec3 n0 = e.Cross(Vec3::right).Normalized();
			Vec3 n1 = e.Cross(Vec3::up).Normalized();
			Vec3 n2 = e.Cross(Vec3::forward).Normalized();
			if (n0.Dot(n0) > 1e-6f) axes[axisCount++] = n0;
			if (n1.Dot(n1) > 1e-6f) axes[axisCount++] = n1;
			if (n2.Dot(n2) > 1e-6f) axes[axisCount++] = n2;
		}

		float tEnter = 0.0f; // global [0,1]
		float tExit  = 1.0f;
		Vec3  bestNrm{0, 0, 0};

		/* ---------- 3. 各軸で連続衝突テスト ---------- */
		for (int a = 0; a < axisCount; ++a) {
			Vec3 A = axes[a];
			if (A.Dot(A) < 1e-8f) continue; // 0 ベクトルはスキップ

			/* ボックス：中心投影 ± radius */
			float projC0 = C0.Dot(A);
			float rBox = fabsf(A.x) * H.x + fabsf(A.y) * H.y + fabsf(A.z) * H.z;
			float minBox0 = projC0 - rBox;
			float maxBox0 = projC0 + rBox;

			/* 三角形：3頂点投影 min/max */
			float v0     = tri.v0.Dot(A);
			float v1     = tri.v1.Dot(A);
			float v2     = tri.v2.Dot(A);
			float minTri = std::min(v0, std::min(v1, v2));
			float maxTri = std::max(v0, std::max(v1, v2));

			/* 相対速度を投影 */
			float vRel = V.Dot(A);

			if (fabsf(vRel) < 1e-8f) {
				/* 平行移動しない軸：現時点で分離なら衝突なし */
				if (maxBox0 < minTri || minBox0 > maxTri) return false;
				/* 重なっているので t=0~1 全域で交差、次の軸へ */
				continue;
			}

			/* 入・出 時間を求める（符号で場合分け） */
			float t0 = (minTri - maxBox0) / vRel; // ボックスが入る時刻
			float t1 = (maxTri - minBox0) / vRel; // ボックスが出る時刻
			if (t0 > t1) std::swap(t0, t1);

			/* 時間窓を更新 */
			if (t0 > tEnter) {
				tEnter  = t0;
				bestNrm = A * (vRel > 0 ? -1.f : 1.f);
			}
			tExit = std::min(t1, tExit);

			/* 窓が壊れたら分離決定 */
			if (tEnter > tExit) return false;
			if (tExit < 0.0f) return false;  // 全体が負
			if (tEnter > 1.0f) return false; // 全体が 1 より後
		}

		/* ---------- 4. 成功 ---------- */
		outTOI = std::max(0.0f, tEnter);
		outNrm = bestNrm.Normalized();
		return true;
	}

	// NOT WORKING YET
	bool SweptSphereVsTriSAT(
		const Vec3&     center,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	) {
		/* ---------- 1. 基本量 ---------- */
		const Vec3 C0 = center;
		const Vec3 V  = delta;

		/* ---------- 2. 分離軸を列挙（Voronoi領域を考慮） ---------- */
		Vec3 axes[10]; // 最大10軸
		int  axisCount = 0;

		/* 2-1 三角形法線（最優先軸） */
		Vec3 triNormal = (tri.v1 - tri.v0).Cross(tri.v2 - tri.v0).Normalized();
		if (triNormal.Dot(triNormal) > 1e-8f) {
			axes[axisCount++] = triNormal;
		}

		/* 2-2 球と三角形のVoronoi領域に基づく軸選択 */
		Vec3 edges[3] = {tri.v1 - tri.v0, tri.v2 - tri.v1, tri.v0 - tri.v2};
		Vec3 verts[3] = {tri.v0, tri.v1, tri.v2};

		// 各頂点に対する処理
		for (int i = 0; i < 3; ++i) {
			Vec3 toVert = verts[i] - C0;

			// 頂点のVoronoi領域かチェック
			Vec3 edge1 = edges[(i + 2) % 3]; // 前のエッジ（逆向き）
			Vec3 edge2 = edges[i];           // 次のエッジ

			bool inVertexRegion = (toVert.Dot(-edge1) >= 0) && (toVert.
				Dot(edge2) >= 0);

			if (inVertexRegion) {
				float len = toVert.Length();
				if (len > 1e-8f) {
					axes[axisCount++] = toVert / len;
				}
			}
		}

		// 各エッジに対する処理
		for (int i = 0; i < 3; ++i) {
			Vec3  edge     = edges[i];
			float edgeLen2 = edge.Dot(edge);
			if (edgeLen2 < 1e-10f) continue;

			Vec3  toStart = C0 - verts[i];
			float t       = toStart.Dot(edge) / edgeLen2;

			// エッジのVoronoi領域かチェック
			if (t >= 0.0f && t <= 1.0f) {
				Vec3  closestPoint = verts[i] + edge * t;
				Vec3  toClosest    = C0 - closestPoint;
				float len          = toClosest.Length();

				if (len > 1e-8f) {
					axes[axisCount++] = toClosest / len;
				}
			}
		}

		float tEnter = 0.0f;
		float tExit  = 1.0f;
		Vec3  bestNrm{0, 0, 0};
		float bestSeparation   = -FLT_MAX; // 最大分離距離を追跡（負の値は侵入を意味する）
		bool  foundValidNormal = false;

		/* ---------- 3. 各軸で連続衝突テスト ---------- */
		for (int a = 0; a < axisCount; ++a) {
			Vec3 A = axes[a];
			if (A.Dot(A) < 1e-8f) continue;

			/* 球体：中心投影 ± radius */
			float projC0     = C0.Dot(A);
			float minSphere0 = projC0 - radius;
			float maxSphere0 = projC0 + radius;

			/* 三角形：3頂点投影 min/max */
			float v0     = tri.v0.Dot(A);
			float v1     = tri.v1.Dot(A);
			float v2     = tri.v2.Dot(A);
			float minTri = std::min(v0, std::min(v1, v2));
			float maxTri = std::max(v0, std::max(v1, v2));

			/* 相対速度を投影 */
			float vRel = V.Dot(A);

			// 分離距離を計算（正の値は分離、負の値は侵入）
			float separation1 = minTri - maxSphere0; // 三角形が球体の上側にある場合
			float separation2 = minSphere0 - maxTri; // 三角形が球体の下側にある場合
			float separation  = std::max(separation1, separation2);

			if (fabsf(vRel) < 1e-8f) {
				/* 静的分離チェック */
				if (separation > 0.0f) return false; // 完全に分離している

				/* 重複している場合、分離距離をチェック（負の値）*/
				if (separation > bestSeparation) {
					bestSeparation   = separation;
					bestNrm          = (separation1 > separation2) ? A : -A;
					foundValidNormal = true;
				}
				continue;
			}

			/* 入・出 時間を求める */
			float t0 = (minTri - maxSphere0) / vRel;
			float t1 = (maxTri - minSphere0) / vRel;
			if (t0 > t1) std::swap(t0, t1);

			/* 時間窓を更新 */
			if (t0 > tEnter) {
				tEnter           = t0;
				bestNrm          = A * (vRel > 0 ? -1.f : 1.f);
				foundValidNormal = true;
				bestSeparation   = -FLT_MAX; // 動的衝突時はリセット
			}
			tExit = std::min(t1, tExit);

			/* 分離判定 */
			if (tEnter > tExit) return false;
			if (tExit < 0.0f) return false;
			if (tEnter > 1.0f) return false;
		}

		/* ---------- 4. 法線の連続性確保と安定化 ---------- */
		if (!foundValidNormal || bestNrm.Dot(bestNrm) < 1e-8f) {
			bestNrm = triNormal; // フォールバック
		}

		/* 法線の方向を一意に決定するための安定化処理 */
		Vec3 centerAtTOI = C0 + V * std::max(0.0f, tEnter);

		// 三角形の重心計算
		Vec3 triCenter   = (tri.v0 + tri.v1 + tri.v2) / 3.0f;
		Vec3 toTriCenter = triCenter - centerAtTOI;

		// 法線が三角形の表面から球体中心を向くように調整
		if (bestNrm.Dot(toTriCenter) > 0) {
			bestNrm = -bestNrm;
		}

		// 三角形法線との一貫性チェック（面接触時の安定性向上）
		float normalAlignment = bestNrm.Dot(triNormal);
		if (fabsf(normalAlignment) > 0.8f) {
			// 法線がほぼ同じ向きの場合
			bestNrm = triNormal * (normalAlignment > 0 ? 1.0f : -1.0f);
		}

		outTOI    = std::max(0.0f, tEnter);
		outNormal = bestNrm.Normalized();
		return true;
	}


	bool SweptCylinderVsTriSAT(
		const Vec3&     base,
		float           halfHeight,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	) {
		base, halfHeight, radius, delta, tri, outTOI, outNormal;
		return false;
	}

	bool SweptCapsuleVsTriSAT(
		const Vec3&     a,
		const Vec3&     b,
		float           radius,
		const Vec3&     delta,
		const Triangle& tri,
		float&          outTOI,
		Vec3&           outNormal
	) {
		a, b, radius, delta, tri, outTOI, outNormal;
		return false;
	}

	bool BoxVsTriangleOverlap(
		const Box&      box,
		const Triangle& tri,
		Vec3&           outSeparationAxis,
		float&          outPenetrationDepth
	) {
		/* ---------- 1. 基本量 ---------- */
		const Vec3 C = box.center;
		const Vec3 H = box.half;

		/* ---------- 2. 13 軸を列挙（SweptAabbVsTriSATと同様） ---------- */
		Vec3 axes[13];
		int  axisCount = 0;

		/* 2-1 三角形法線 */
		Vec3 triNormal = (tri.v1 - tri.v0).Cross(tri.v2 - tri.v0);
		if (triNormal.Dot(triNormal) > 1e-8f) {
			axes[axisCount++] = triNormal.Normalized();
		}

		/* 2-2 ボックス軸 (世界 XYZ) */
		axes[axisCount++] = {1, 0, 0};
		axes[axisCount++] = {0, 1, 0};
		axes[axisCount++] = {0, 0, 1};

		/* 2-3 triEdge × boxAxis (交叉積) */
		Vec3 e0          = tri.v1 - tri.v0;
		Vec3 e1          = tri.v2 - tri.v1;
		Vec3 e2          = tri.v0 - tri.v2;
		Vec3 triEdges[3] = {e0, e1, e2};
		for (Vec3 e : triEdges) {
			if (e.Dot(e) < 1e-10f) continue; // degenerate
			Vec3 n0 = e.Cross(Vec3::right).Normalized();
			Vec3 n1 = e.Cross(Vec3::up).Normalized();
			Vec3 n2 = e.Cross(Vec3::forward).Normalized();
			if (n0.Dot(n0) > 1e-6f) axes[axisCount++] = n0;
			if (n1.Dot(n1) > 1e-6f) axes[axisCount++] = n1;
			if (n2.Dot(n2) > 1e-6f) axes[axisCount++] = n2;
		}

		float maxPenetration  = -FLT_MAX;
		Vec3  bestAxis{0, 0, 0};
		bool  foundOverlap    = true;

		/* ---------- 3. 各軸で重なり判定 ---------- */
		for (int a = 0; a < axisCount; ++a) {
			Vec3 A = axes[a];
			if (A.Dot(A) < 1e-8f) continue; // 0 ベクトルはスキップ

			/* ボックス：中心投影 ± radius */
			float projC  = C.Dot(A);
			float rBox   = fabsf(A.x) * H.x + fabsf(A.y) * H.y + fabsf(A.z) * H.z;
			float minBox = projC - rBox;
			float maxBox = projC + rBox;

			/* 三角形：3頂点投影 min/max */
			float v0     = tri.v0.Dot(A);
			float v1     = tri.v1.Dot(A);
			float v2     = tri.v2.Dot(A);
			float minTri = std::min(v0, std::min(v1, v2));
			float maxTri = std::max(v0, std::max(v1, v2));

			/* 分離判定 */
			if (maxBox < minTri || minBox > maxTri) {
				foundOverlap = false;
				break; // 分離軸が見つかった場合、重なりなし
			}

			/* 侵入深度を計算 */
			float penetration1 = maxBox - minTri; // ボックスが三角形に侵入している深度
			float penetration2 = maxTri - minBox; // 三角形がボックスに侵入している深度
			float penetration  = std::min(penetration1, penetration2);

			/* 最小侵入深度を追跡 */
			if (penetration > maxPenetration) {
				maxPenetration = penetration;
				bestAxis       = A;
				// 侵入方向を適切に設定
				if (penetration1 < penetration2) {
					bestAxis = -bestAxis; // ボックスを押し戻す方向
				}
			}
		}

		if (!foundOverlap) {
			return false; // 重なりなし
		}

		/* ---------- 4. 結果を設定 ---------- */
		outSeparationAxis    = bestAxis.Normalized();
		outPenetrationDepth  = maxPenetration;

		return true; // 重なりあり
	}
}
